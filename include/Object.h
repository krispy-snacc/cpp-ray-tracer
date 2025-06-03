#ifndef OBJECT_H
#define OBJECT_H

#include <memory>
#include <limits.h>

#include "Vec3.h"
#include "Ray.h"
#include "Scene.h"
#include "Interval.h"
#include "Utils.h"

class Material;

class HitRecord {
public:
    Point3 hitPoint;
    Vec3 normal;
    double t;
    bool front_face;
    std::shared_ptr<Material> mat;
};

class Object {
public:
    virtual bool RayHit(const Ray& r, HitRecord& hit, Interval ray_t = Interval::Universe) = 0;
};

class Sphere : public Object {
private:
    Vec3 center;
    double radius;
    std::shared_ptr<Material> mat;

public:
    Sphere(const Vec3& center, double radius, std::shared_ptr<Material> mat) : center(center), radius(std::fmax(0, radius)), mat(mat) {};

    bool RayHit(const Ray& r, HitRecord& hit, Interval ray_t = Interval::Universe) {
        Vec3 oc = center - r.origin();
        auto a = r.direction().length_squared();
        auto h = dot(r.direction(), oc);
        auto c = dot(oc, oc) - radius * radius;
        auto discriminant = h * h - a * c;
        if (discriminant < 0) return false;
        double sqrtd = std::sqrt(discriminant);

        double root = (h - sqrtd) / a;
        if (!ray_t.surrounds(root)) {
            root = (h + sqrtd) / a;
            if (!ray_t.surrounds(root))
                return false;
        }

        hit.t = root;
        hit.hitPoint = r.at(hit.t);
        Vec3 outward_normal = (hit.hitPoint - center) / radius;
        bool front_face;
        if (dot(r.direction(), outward_normal) > 0.0) {
            // ray is inside the sphere
            hit.normal = -outward_normal;
            front_face = false;
        }
        else {
            // ray is outside the sphere
            hit.normal = outward_normal;
            front_face = true;
        }
        hit.front_face = front_face;
        hit.mat = mat;


        return true;
    }

};

inline std::shared_ptr<Object> MakeSphere(const Vec3& center, double radius, std::shared_ptr<Material> mat) {
    return std::make_shared<Sphere>(center, radius, mat);
}


#endif