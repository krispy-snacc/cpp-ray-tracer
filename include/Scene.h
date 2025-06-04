#ifndef SCENE_H
#define SCENE_H

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <iomanip> // for std::setprecision
#include <filesystem>  // C++17

namespace fs = std::filesystem;


#include "Vec3.h"
#include "Color.h"
#include "Ray.h"
#include "Object.h"
#include "Material.h"
#include "Utils.h"

std::mutex console_mutex; // Global or static to protect console output

class Scene {
public:
    double canvas_height;
    double canvas_width;
    Interval clip_interval = Interval(0.001, infinity);
    int samples_per_pixel = 20;
    int max_depth = 10;
    double vfov = 90;
    Point3 lookfrom = Point3(0, 0, 0);
    Point3 lookat = Point3(0, 0, -1);
    Vec3 vup = Vec3(0, 1, 0);
    double defocus_angle = 0;
    double focus_dist = 10;

private:
    Point3 camera_center;
    double viewport_height;
    double viewport_width;
    Point3 viewport_upper_left;
    Point3 pixel00_loc;             // Location of pixel 0, 0
    Vec3 viewport_u;
    Vec3 viewport_v;
    Vec3 pixel_delta_u;             // Offset to pixel to the right
    Vec3 pixel_delta_v;             // Offset to pixel below
    Vec3 u, v, w;                   // Camera frame basis vectors
    Vec3 defocus_disk_u;            // Defocus disk horizontal radius
    Vec3 defocus_disk_v;            // Defocus disk vertical radius

    double pixel_samples_scale;

    std::vector<std::shared_ptr<Object>> objects;
    std::vector<Color> frame;

public:
    Scene() {}

    void Init() {

        camera_center = lookfrom;
        // focal_length = (lookfrom - lookat).length();

        // Calculate the u,v,w unit basis vectors for the camera coordinate frame.
        w = normalize(lookfrom - lookat);
        u = normalize(cross(vup, w));
        v = cross(w, u);


        int frame_size = canvas_height * canvas_width;

        // Determine viewport dimensions.
        double theta = degrees_to_radians(vfov);
        double h = std::tan(theta / 2);
        viewport_height = 2 * h * focus_dist;
        viewport_width = viewport_height * (double(canvas_width) / canvas_height);

        viewport_v = viewport_height * -v;  // Vector down viewport vertical edge
        viewport_u = viewport_width * u;    // Vector across viewport horizontal edge

        // Pixel distances horizontal and vertical
        pixel_delta_u = viewport_u / canvas_width;
        pixel_delta_v = viewport_v / canvas_height;

        // Upper left most pixel
        viewport_upper_left = camera_center - (focus_dist * w) - viewport_u / 2 - viewport_v / 2;;
        pixel00_loc = viewport_upper_left + (pixel_delta_u + pixel_delta_v) / 2;
        pixel_samples_scale = 1.0 / samples_per_pixel;

        // Calculate the camera defocus disk basis vectors.
        auto defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;
    }

    void AddObject(std::shared_ptr<Object> obj) {
        objects.push_back(std::move(obj));
    }


    void Render() {
        frame.resize(canvas_height * canvas_width);

        unsigned int thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) thread_count = 4;

        std::vector<std::thread> threads;
        int rows_per_thread = canvas_height / thread_count;

        std::atomic<int> lines_done(0);

        auto render_rows = [&](int start_row, int end_row) {
            for (int j = start_row; j < end_row; j++) {
                for (int i = 0; i < canvas_width; i++) {
                    int index = j * canvas_width + i;
                    frame[index] = samplePixel(i, j);
                }

                int completed = lines_done.fetch_add(1) + 1;

                // Show progress every N lines
                if (completed % 10 == 0 || completed == canvas_height) {
                    std::lock_guard<std::mutex> lock(console_mutex);
                    double percent = (double)completed / canvas_height * 100.0;
                    std::clog << "\rProgress: " << std::fixed << std::setprecision(1)
                        << percent << "% (" << completed << "/" << canvas_height << ")"
                        << std::flush;
                }
            }
            };

        for (unsigned int t = 0; t < thread_count; ++t) {
            int start_row = t * rows_per_thread;
            int end_row = (t == thread_count - 1) ? canvas_height : start_row + rows_per_thread;
            threads.emplace_back(render_rows, start_row, end_row);
        }

        for (auto& t : threads) {
            t.join();
        }

        {
            std::lock_guard<std::mutex> lock(console_mutex);
            std::clog << "\rProgress: 100.0% (" << canvas_height << "/" << canvas_height << ")"
                << " - Done.           \n";
        }
    }

    void Write(fs::path output_path) {
        int write_buffer_size = canvas_width * canvas_height * 3;
        unsigned char* write_buffer = new unsigned char[write_buffer_size];

        for (int j = 0; j < canvas_height; ++j) {
            for (int i = 0; i < canvas_width; ++i) {
                int idx = j * canvas_width + i;

                double r = frame[idx].x();
                double g = frame[idx].y();
                double b = frame[idx].z();

                auto tone = [](double x) { return x / (1.0 + x); };   // maps [0,∞) → [0,1)

                r = tone(r);
                g = tone(g);
                b = tone(b);

                r = linear_to_gamma(r);   // γ-correct afterwards
                g = linear_to_gamma(g);
                b = linear_to_gamma(b);

                Interval col_range(0.0, 0.999);
                // safe cast
                write_buffer[idx * 3 + 0] = static_cast<unsigned char>(256 * col_range.clamp(r));
                write_buffer[idx * 3 + 1] = static_cast<unsigned char>(256 * col_range.clamp(g));
                write_buffer[idx * 3 + 2] = static_cast<unsigned char>(256 * col_range.clamp(b));

            }
        }



        fs::create_directories(output_path.parent_path());
        if (stbi_write_png(output_path.string().c_str(), canvas_width, canvas_height, 3, write_buffer, canvas_width * 3)) {
            std::cout << "Saved " << output_path.string() << std::endl;
        }
        else {
            std::cerr << "Failed to write PNG" << std::endl;
        }
        delete[] write_buffer;
    }

private:
    Color getRayColor(const Ray& r, int depth) {
        if (depth <= 0) return Color(0, 0, 0);

        HitRecord rec;
        bool hit_anything = false;
        double closest_so_far = clip_interval.max;

        for (const auto& obj : objects) {
            HitRecord temp_rec;
            if (obj->RayHit(r, temp_rec, Interval(clip_interval.min, closest_so_far))) {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;
            }
        }

        if (hit_anything) {
            Ray scattered;
            Color emitted;
            Color attenuation;
            bool didAbsorb = false;
            bool didScatter = false;
            bool didEmit = false;

            rec.mat->fall(r, rec, attenuation, scattered, didScatter, didAbsorb, didEmit);
            if (didEmit) emitted = attenuation;

            if (didScatter)
                return attenuation * getRayColor(scattered, depth - 1);
            else if (didEmit)
                return attenuation;  // Emission color
            return Color(0, 0, 0);
        }

        Vec3 unit_direction = normalize(r.direction());
        double t = (unit_direction.y() + 1.0) / 2.0;
        double exposure = 0.05;
        return lerp(Vec3(1, 1, 1) * exposure, Vec3(0.5, 0.7, 1) * exposure, t);
    }


    Ray getRay(int i, int j) const {
        // Construct a camera ray originating from the origin and directed at randomly sampled
        // point around the pixel location i, j.

        auto offset = sampleSquare();
        auto pixel_sample = pixel00_loc
            + ((i + offset.x()) * pixel_delta_u)
            + ((j + offset.y()) * pixel_delta_v);

        auto ray_origin = (defocus_angle <= 0) ? camera_center : defocus_disk_sample();
        auto ray_direction = pixel_sample - ray_origin;

        return Ray(ray_origin, ray_direction);
    }

    Vec3 sampleSquare() const {
        // Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit square.
        return Vec3(random_double() - 0.5, random_double() - 0.5, 0);
    }

    Color samplePixel(int i, int j) {
        Color pixel_color(0, 0, 0);
        for (int sample = 0; sample < samples_per_pixel; sample++) {
            Ray r = getRay(i, j);
            pixel_color = pixel_color + getRayColor(r, max_depth);
        }
        return pixel_samples_scale * pixel_color;
    }

    Point3 defocus_disk_sample() const {
        // Returns a random point in the camera defocus disk.
        Vec3 p = random_in_unit_disk();
        return camera_center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
    }
};


#endif