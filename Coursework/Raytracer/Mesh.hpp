#pragma once
#include "Renderable.hpp"
#include "GeomUtil.hpp"
#include "Model.hpp"

/// <summary>
/// A triangle mesh. Intersections are found by testing triangles in either the
/// full Model or in an optional partial face list. Partial face lists are used
/// by BVH nodes and by OBJ material groups.
/// </summary>
class Mesh : public Renderable
{
private:
    AABB aabb_;
    std::vector<std::vector<VertexIndices>> indexList_;
protected:
    const Model* model_;
    bool culling_, checkAABB_;
public:
    Mesh(const Shader* shader, const Model* model,
        const std::vector<std::vector<VertexIndices>>* indexList = nullptr,
        bool culling = true, bool checkAABB = true, IntersectMask mask = DEFAULT_BITMASK)
        :Renderable(shader, mask), model_(model), culling_(culling), checkAABB_(checkAABB)
    {
        if (indexList) {
            indexList_ = std::vector<std::vector<VertexIndices>>(*indexList);
        }
        computeAABB();
    }

    int nfaces() const
    {
        if (indexList_.size() >= 1)
            return static_cast<int>(indexList_.size());
        else
            return model_->nfaces();
    }

    std::vector<VertexIndices> getFace(int f) const
    {
        if (indexList_.size() >= 1)
            return indexList_[f];
        else
            return model_->face(f);
    }

    virtual bool intersect(const Ray& ray, float minT, float maxT, HitInfo& info, IntersectMask mask) const override
    {
        if (!checkMask(mask)) return false;
        if (checkAABB_ && !aabb_.intersect(ray, minT, maxT)) return false;

        float closestT = std::numeric_limits<float>::max();

        for (int f = 0; f < nfaces(); ++f) {
            std::vector<VertexIndices> face = getFace(f);
            if (face.size() < 3) continue;

            Eigen::Vector3f v0 = model_->vert(face[0].vert);
            Eigen::Vector3f v1 = model_->vert(face[1].vert);
            Eigen::Vector3f v2 = model_->vert(face[2].vert);

            Eigen::Vector3f v0World = transformPosition(Entity::modelToWorld(), v0);
            Eigen::Vector3f v1World = transformPosition(Entity::modelToWorld(), v1);
            Eigen::Vector3f v2World = transformPosition(Entity::modelToWorld(), v2);

            Eigen::Vector3f v0v1 = v1World - v0World;
            Eigen::Vector3f v0v2 = v2World - v0World;
            Eigen::Vector3f pvec = ray.direction.cross(v0v2);
            float det = v0v1.dot(pvec);

            if (culling_) {
                if (det < 1e-6f) continue;
            }
            else {
                if (fabs(det) < 1e-6f) continue;
            }

            float invDet = 1.0f / det;
            Eigen::Vector3f tvec = ray.origin - v0World;
            float u = tvec.dot(pvec) * invDet;
            if (u < 0.0f || u > 1.0f) continue;

            Eigen::Vector3f qvec = tvec.cross(v0v1);
            float v = ray.direction.dot(qvec) * invDet;
            if (v < 0.0f || u + v > 1.0f) continue;

            float t = v0v2.dot(qvec) * invDet;
            if (t >= closestT) continue;
            if (t < minT || t > maxT) continue;

            info.hitT = t;
            info.inDirection = ray.direction;
            info.location = ray.origin + t * ray.direction;
            info.shader = shader();

            if (model_->hasNormals() && face[0].norm >= 0 && face[1].norm >= 0 && face[2].norm >= 0) {
                Eigen::Vector3f vn0 = transformNormal(Entity::modelToWorld(), model_->normal(face[0].norm));
                Eigen::Vector3f vn1 = transformNormal(Entity::modelToWorld(), model_->normal(face[1].norm));
                Eigen::Vector3f vn2 = transformNormal(Entity::modelToWorld(), model_->normal(face[2].norm));
                info.normal = ((1.0f - (u + v)) * vn0 + u * vn1 + v * vn2).normalized();
            }
            else {
                info.normal = v0v1.cross(v0v2).normalized();
            }

            if (model_->hasTexCoords() && face[0].tex >= 0 && face[1].tex >= 0 && face[2].tex >= 0) {
                Eigen::Vector2f vt0 = model_->texCoord(face[0].tex);
                Eigen::Vector2f vt1 = model_->texCoord(face[1].tex);
                Eigen::Vector2f vt2 = model_->texCoord(face[2].tex);
                info.texCoords = (1.0f - (u + v)) * vt0 + u * vt1 + v * vt2;
            }
            else {
                info.texCoords = Eigen::Vector2f(0.0f, 0.0f);
            }

            closestT = t;
        }

        return closestT != std::numeric_limits<float>::max();
    }

    void computeAABB()
    {
        for (int i = 0; i < 3; ++i) {
            aabb_.min[i] = std::numeric_limits<float>::max();
            aabb_.max[i] = std::numeric_limits<float>::lowest();
        }

        for (int f = 0; f < nfaces(); ++f) {
            std::vector<VertexIndices> face = getFace(f);
            for (int v = 0; v < 3; ++v) {
                Eigen::Vector3f v0 = model_->vert(face[v].vert);
                v0 = transformPosition(Entity::modelToWorld(), v0);
                for (int i = 0; i < 3; ++i) {
                    if (v0[i] < aabb_.min[i]) aabb_.min[i] = v0[i];
                    if (v0[i] > aabb_.max[i]) aabb_.max[i] = v0[i];
                }
            }
        }
    }

    virtual void modelToWorld(const Eigen::Matrix4f& m) override
    {
        Entity::modelToWorld(m);
        computeAABB();
    }

    virtual AABB getAABB() const override { return aabb_; }
    virtual std::string print() const override { return "Mesh"; }
};
