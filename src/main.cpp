#include <iostream>

#include "Scene.h"
#include "Object.h"
#include "Material.h"

int main() {

    // Image
    auto aspect_ratio = 16.0 / 9.0;
    int image_width = 1920;

    // Calculate the image height, and ensure that it's at least 1.
    int image_height = int(image_width / aspect_ratio);
    image_height = (image_height < 1) ? 1 : image_height;

    double focal_length = 1.0;

    Scene scene;

    scene.canvas_height = image_height;
    scene.canvas_width = image_width;
    scene.samples_per_pixel = 100;
    scene.max_depth = 100;

    scene.vfov = 20;
    scene.lookfrom = Point3(13, 2, 3);
    scene.lookat = Point3(0, 0, 0);

    scene.defocus_angle = 0.6;
    scene.focus_dist = 10.0;

    scene.Init();


    auto ground_material = MakeLambertian(Color(0.5, 0.5, 0.5));
    scene.AddObject(MakeSphere(Point3(0, -1000, 0), 1000, ground_material));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_double();
            Point3 center(a + 0.9 * random_double(), 0.2, b + 0.9 * random_double());

            if ((center - Point3(4, 0.2, 0)).length() > 0.9) {
                std::shared_ptr<Material> sphere_material;

                if (choose_mat < 0.5) {
                    // diffuse
                    auto albedo = Color::random() * Color::random();
                    sphere_material = MakeLambertian(albedo);
                    scene.AddObject(MakeSphere(center, 0.2, sphere_material));
                }
                else if (choose_mat < 0.8) {
                    // emission
                    auto emit_color = from_hsv(random_double(), 0.7, 1);
                    emit_color = emit_color * emit_color;
                    sphere_material = MakeEmission(emit_color, random_double(6.0, 20.0));
                    scene.AddObject(MakeSphere(center, 0.2, sphere_material));
                }
                else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = Color::random(0.5, 1);
                    auto fuzz = random_double(0, 0.5);
                    sphere_material = MakeMetal(albedo, fuzz);
                    scene.AddObject(MakeSphere(center, 0.2, sphere_material));
                }
                else {
                    // glass
                    sphere_material = MakeDielectric(1.5);
                    scene.AddObject(MakeSphere(center, 0.2, sphere_material));
                }
            }
        }
    }

    auto material1 = MakeDielectric(1.5);
    scene.AddObject(MakeSphere(Point3(0, 1, 0), 1.0, material1));

    auto material2 = MakeLambertian(Color(0.4, 0.2, 0.1));
    scene.AddObject(MakeSphere(Point3(-4, 1, 0), 1.0, material2));

    auto material3 = MakeMetal(Color(0.7, 0.6, 0.5), 0.0);
    scene.AddObject(MakeSphere(Point3(4, 1, 0), 1.0, material3));



    auto material_ground = MakeLambertian(Color(0.1, 0.2, 0.5));
    auto material_center = MakeLambertian(Color(0.1, 0.2, 0.5));
    auto material_left = MakeDielectric(1.5);
    auto material_bubble = MakeDielectric(1.0 / 1.5);
    auto material_right = MakeMetal(Color(0.8, 0.6, 0.2), 1.0);

    scene.AddObject(MakeSphere(Point3(0.0, -100.5, -1.0), 100.0, material_ground));
    scene.AddObject(MakeSphere(Point3(0.0, 0.0, -1.2), 0.5, material_center));
    scene.AddObject(MakeSphere(Point3(-1.0, 0.0, -1.0), 0.5, material_left));
    scene.AddObject(MakeSphere(Point3(-1.0, 0.0, -1.0), 0.4, material_bubble));
    scene.AddObject(MakeSphere(Point3(1.0, 0.0, -1.0), 0.5, material_right));


    scene.Render();
    scene.Write("image.png");
    return 0;
}