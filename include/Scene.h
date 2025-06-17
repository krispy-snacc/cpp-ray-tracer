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
#include <algorithm> 

namespace fs = std::filesystem;


#include "Vec3.h"
#include "Color.h"
#include "Ray.h"
#include "Object.h"
#include "Material.h"
#include "Utils.h"

std::mutex console_mutex; // Global or static to protect console output


class PixelInfo {
public:
    Color color;
    Color albedo;
    Vec3 normal;
    double depth;
};

class Scene {
public:
    int canvas_height;
    int canvas_width;
    Interval clip_interval = Interval(0.001, infinity);
    int samples_per_pixel = 20;
    int max_bouces = 10;
    double vfov = 90;
    Point3 lookfrom = Point3(0, 0, 0);
    Point3 lookat = Point3(0, 0, -1);
    Vec3 vup = Vec3(0, 1, 0);
    double defocus_angle = 0;
    double focus_dist = 10;
    double exposure = 1;

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
    std::vector<Color> color_map;
    std::vector<Color> albedo_map;
    std::vector<Vec3> normal_map;
    std::vector<double> depth_map;

    std::vector<std::shared_ptr<Object>> objects;
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
        color_map.assign(canvas_height * canvas_width, Color(0, 0, 0));
        albedo_map.assign(canvas_height * canvas_width, Color(0, 0, 0));
        normal_map.assign(canvas_height * canvas_width, Vec3(0, 0, 0));
        depth_map.assign(canvas_height * canvas_width, 0.0);

        unsigned int thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) thread_count = 4;

        std::vector<std::thread> threads;
        int rows_per_thread = canvas_height / thread_count;

        std::atomic<int> lines_done(0);

        auto render_rows = [&](int start_row, int end_row) {
            for (int j = start_row; j < end_row; j++) {
                for (int i = 0; i < canvas_width; i++) {
                    int index = j * canvas_width + i;

                    PixelInfo pixel;

                    samplePixel(i, j, pixel);

                    color_map[index] = pixel.color;
                    albedo_map[index] = pixel.albedo;
                    normal_map[index] = pixel.normal;
                    depth_map[index] = pixel.depth;

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

    void Write(fs::path output_path, std::vector<double> d_buffer) {
        int write_buffer_size = canvas_width * canvas_height * 3;
        unsigned char* write_buffer = new unsigned char[write_buffer_size];

        const double max_depth_valid = 1e5;
        const double epsilon = 1e-4;

        for (auto& d : d_buffer) {
            if (!std::isfinite(d) || d > max_depth_valid) {
                d = max_depth_valid;
            }
            if (d < epsilon) {
                d = epsilon;
            }
        }

        auto [min_d_it, max_d_it] = std::minmax_element(d_buffer.begin(), d_buffer.end());


        for (int j = 0; j < canvas_height; j++) {
            for (int i = 0; i < canvas_width; i++) {
                int idx = j * canvas_width + i;

                double r, g, b;
                double d = d_buffer[idx];

                // Avoid log(0)
                const double epsilon = 1e-4;
                d = std::max(d, epsilon);

                // Log scale normalization
                double log_d = std::log(d);
                double log_min = std::log(*min_d_it + epsilon);
                double log_max = std::log(*max_d_it + epsilon);
                double scaled = (log_d - log_min) / (log_max - log_min);

                r = g = b = scaled;

                r = linear_to_gamma(r);   // gamma-correct afterwards
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

    void Write(fs::path output_path, std::vector<Color> color_buffer) {
        int write_buffer_size = canvas_width * canvas_height * 3;
        unsigned char* write_buffer = new unsigned char[write_buffer_size];

        for (int j = 0; j < canvas_height; j++) {
            for (int i = 0; i < canvas_width; i++) {
                int idx = j * canvas_width + i;

                double r = color_buffer[idx].x();
                double g = color_buffer[idx].y();
                double b = color_buffer[idx].z();

                auto tone = [](double x) { return x / (1.0 + x); };   // maps [0,inf) → [0,1)

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
    void getRayHit(const Ray& r, int bounce_depth, PixelInfo& pixel) {
        if (bounce_depth <= 0) {
            pixel.color = Color(0, 0, 0);
            return;
        }

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
            Color emitted = Color(0, 0, 0); // Always initialize
            Color attenuation;
            Color albedo;
            bool didScatter = false;
            bool didEmit = false;

            rec.mat->fall(r, rec, attenuation, albedo, scattered, didScatter, didEmit);

            pixel.albedo = albedo;
            pixel.normal = rec.normal;
            pixel.depth = rec.t;

            if (didEmit)
                emitted = attenuation; // attenuation is emission color

            if (didScatter) {
                PixelInfo pixel2;
                getRayHit(scattered, bounce_depth - 1, pixel2);
                pixel.color = emitted + attenuation * pixel2.color;
                return;
            }
            else {
                pixel.color = emitted;  // Emission color
            }
            return;
        }

        Vec3 unit_direction = normalize(r.direction());
        double t = (unit_direction.y() + 1.0) / 2.0;
        pixel.color = lerp(Vec3(1, 1, 1) * exposure, Vec3(0.5, 0.7, 1) * exposure, t);
        pixel.albedo = Vec3();
        pixel.normal = Vec3();
        pixel.depth = clip_interval.max;
        return;
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

    void samplePixel(int i, int j, PixelInfo& pixel) {
        PixelInfo pixel1;
        pixel1.depth = 0.0; // Ensure depth is initialized

        for (int sample = 0; sample < samples_per_pixel; sample++) {
            Ray r = getRay(i, j);
            PixelInfo pixel2;
            getRayHit(r, max_bouces, pixel2);
            pixel1.color = pixel1.color + pixel2.color;
            pixel1.albedo = pixel1.albedo + pixel2.albedo;
            pixel1.normal = pixel1.normal + pixel2.normal;
            pixel1.depth += pixel2.depth;
        }

        pixel.color = pixel_samples_scale * pixel1.color;
        pixel.albedo = pixel_samples_scale * pixel1.albedo;
        pixel.normal = pixel_samples_scale * pixel1.normal;
        pixel.depth = pixel_samples_scale * pixel1.depth;
        return;
    }

    Point3 defocus_disk_sample() const {
        // Returns a random point in the camera defocus disk.
        Vec3 p = random_in_unit_disk();
        return camera_center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
    }
public:
    std::vector<Color> get_color_map() const {
        return color_map;
    }

    std::vector<Color> get_albedo_map() const {
        return albedo_map;
    }

    std::vector<Vec3> get_normal_map() const {
        return normal_map;
    }

    std::vector<double> get_depth_map() const {
        return depth_map;
    }
};


#endif