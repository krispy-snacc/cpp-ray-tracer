#ifndef MATERIAL_H
#define MATERIAL_H

#include "Object.h"

class Material {
public:
    virtual ~Material() = default;

    virtual bool scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const {
        return false;
    }
};



class Lambertian : public Material {
private:
    Color albedo;
public:
    Lambertian(const Color& albedo) : albedo(albedo) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const override {
        Vec3 scatter_direction = rec.normal + random_unit_vector();
        if (scatter_direction.near_zero())
            scatter_direction = rec.normal;
        scattered = Ray(rec.hitPoint, scatter_direction);
        attenuation = albedo;
        return true;
    }

};

inline std::shared_ptr<Material> MakeLambertian(const Color& albedo) {
    return std::make_shared<Lambertian>(albedo);
}

class Metal : public Material {
private:
    Color albedo;
    double fuzz;
public:
    Metal(const Color& albedo, double fuzz) : albedo(albedo), fuzz(fuzz) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const override {
        Vec3 reflected = reflect(r_in.direction(), rec.normal);
        reflected = normalize(reflected) + (fuzz * random_unit_vector());
        scattered = Ray(rec.hitPoint, reflected);
        attenuation = albedo;
        return (dot(scattered.direction(), rec.normal) > 0);
    }

};

inline std::shared_ptr<Material> MakeMetal(const Color& albedo, double fuzz) {
    return std::make_shared<Metal>(albedo, fuzz);
}

class Dielectric : public Material {
private:
    double refractive_index;
public:
    Dielectric(double refractive_index) : refractive_index(refractive_index) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const override {
        attenuation = Color(1.0, 1.0, 1.0);
        double ri = rec.front_face ? (1.0 / refractive_index) : refractive_index;

        Vec3 unit_direction = normalize(r_in.direction());
        double cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);

        bool cannot_refract = ri * sin_theta > 1.0;
        Vec3 direction;

        if (cannot_refract || reflectance(cos_theta, ri) > random_double())
            direction = reflect(unit_direction, rec.normal);
        else
            direction = refract(unit_direction, rec.normal, ri);

        scattered = Ray(rec.hitPoint, direction);
        return true;
    }
private:
    static double reflectance(double cosine, double refraction_index) {
        // Use Schlick's approximation for reflectance.
        auto r0 = (1 - refraction_index) / (1 + refraction_index);
        r0 = r0 * r0;
        return r0 + (1 - r0) * std::pow((1 - cosine), 5);
    }

};

inline std::shared_ptr<Material> MakeDielectric(double refractive_index) {
    return std::make_shared<Dielectric>(refractive_index);
}


#endif