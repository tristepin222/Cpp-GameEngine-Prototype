#pragma once
#include "ecs/System.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/RigidBody.hpp"
#include "ecs/components/Collider.hpp"
#include "ecs/components/Hierarchy.hpp"
#include "editor/EditorModeState.hpp"
#include <glm/glm.hpp>
#include <algorithm>
#include <vector>
#include <cmath>
#include <limits>

namespace Engine {

    struct CollisionInfo {
        bool collided = false;
        glm::vec3 normal{0.0f}; // Points from A to B
        float penetration = 0.0f;
        glm::vec3 contactPoint{0.0f};
    };

    struct Ray {
        glm::vec3 origin;
        glm::vec3 direction; // Normalized
    };

    struct RaycastHit {
        bool hit = false;
        Entity entity;
        float distance = std::numeric_limits<float>::max();
        glm::vec3 position{0.0f};
        glm::vec3 normal{0.0f};
    };

    /**
     * @class PhysicsSystem
     * @brief Evaluates rigid body movement and resolves collider intersections.
     */
    class PhysicsSystem : public System {
    public:
        PhysicsSystem(Registry& reg, EditorModeState& editorMode)
            : registry(reg), editorMode(editorMode) {}

        void update(float dt) override {
            if (!editorMode.isPlaying) return;
            if (dt <= 0.0f) return;

            glm::vec3 gravityVector{0.0f, -9.81f, 0.0f};

            // 1. Integration Pass (Gravity and Force accumulation)
            for (auto [entity, transform, rb] : registry.view<Transform, RigidBodyComponent>()) {
                if (rb.type == RigidBodyType::Static) continue;

                // Validate mass to prevent NaN propagation
                float massVal = rb.mass;
                if (massVal < 1e-4f || std::isnan(massVal) || std::isinf(massVal)) {
                    rb.mass = 1.0f;
                    massVal = 1.0f;
                }

                glm::vec3 gravityForce = gravityVector * rb.gravityScale * massVal;
                glm::vec3 totalForce = rb.force + gravityForce;
                glm::vec3 accel = totalForce / massVal;

                // Validate acceleration
                if (std::isnan(accel.x) || std::isinf(accel.x)) accel = glm::vec3(0.0f);

                rb.velocity += accel * dt;

                // Apply linear drag damping
                rb.velocity *= glm::clamp(1.0f - rb.linearDrag * dt, 0.0f, 1.0f);

                // Validate velocity
                if (std::isnan(rb.velocity.x) || std::isinf(rb.velocity.x)) rb.velocity = glm::vec3(0.0f);

                transform.position += rb.velocity * dt;

                // Validate position to prevent visual/matrix freezes
                if (std::isnan(transform.position.x) || std::isinf(transform.position.x) ||
                    std::isnan(transform.position.y) || std::isinf(transform.position.y) ||
                    std::isnan(transform.position.z) || std::isinf(transform.position.z)) {
                    transform.position = glm::vec3(0.0f);
                    rb.velocity = glm::vec3(0.0f);
                }

                // Rotational integration
                if (std::isnan(rb.angularVelocity.x) || std::isinf(rb.angularVelocity.x) ||
                    std::isnan(rb.angularVelocity.y) || std::isinf(rb.angularVelocity.y) ||
                    std::isnan(rb.angularVelocity.z) || std::isinf(rb.angularVelocity.z)) {
                    rb.angularVelocity = glm::vec3(0.0f);
                }

                // Calculate angular acceleration from torque (using simplified local inertia tensor for a unit sphere/box)
                // To keep it simple and stable, we update angularVelocity directly or via simplified moment of inertia.
                // Let's assume a simplified moment of inertia contribution for torque integration:
                float inertiaScale = 0.5f * massVal;
                glm::vec3 angularAccel = rb.torque / (inertiaScale > 1e-4f ? inertiaScale : 1.0f);
                rb.angularVelocity += angularAccel * dt;

                // Apply angular drag damping
                rb.angularVelocity *= glm::clamp(1.0f - rb.angularDrag * dt, 0.0f, 1.0f);

                // Update Euler rotation (degrees)
                transform.rotation += glm::degrees(rb.angularVelocity) * dt;

                // Keep rotation angles within clean range [-180, 180] or [0, 360] to prevent Euler drift
                transform.rotation.x = std::fmod(transform.rotation.x, 360.0f);
                transform.rotation.y = std::fmod(transform.rotation.y, 360.0f);
                transform.rotation.z = std::fmod(transform.rotation.z, 360.0f);

                // Reset forces and torques
                rb.force = glm::vec3(0.0f);
                rb.torque = glm::vec3(0.0f);
            }

            // 2. Collision Detection & Resolution Pass
            struct ColliderEntry {
                Entity entity;
                Transform* transform;
                ColliderComponent* collider;
            };
            
            std::vector<ColliderEntry> colliders;
            for (auto [entity, transform, collider] : registry.view<Transform, ColliderComponent>()) {
                colliders.push_back({entity, &transform, &collider});
            }

            // Run 2 passes to stabilize collision stacking
            const int passes = 2;
            for (int pass = 0; pass < passes; ++pass) {
                for (size_t i = 0; i < colliders.size(); ++i) {
                    for (size_t j = i + 1; j < colliders.size(); ++j) {
                        auto& entryA = colliders[i];
                        auto& entryB = colliders[j];

                        CollisionInfo colInfo = checkCollision(
                            entryA.entity, *entryA.collider,
                            entryB.entity, *entryB.collider
                        );

                        if (colInfo.collided) {
                            resolveCollision(
                                entryA.entity, *entryA.transform, *entryA.collider,
                                entryB.entity, *entryB.transform, *entryB.collider,
                                colInfo
                            );
                        }
                    }
                }
            }
        }

        /**
         * @brief Casts a ray into the scene and finds the closest intersecting entity with a collider.
         */
        RaycastHit raycast(const Ray& ray) {
            RaycastHit closestHit;
            closestHit.distance = std::numeric_limits<float>::max();

            for (auto [entity, transform, collider] : registry.view<Transform, ColliderComponent>()) {
                glm::mat4 worldM = getEntityWorldMatrix(entity);
                glm::vec3 colPos = glm::vec3(worldM * glm::vec4(collider.offset, 1.0f));
                RaycastHit hit;

                if (collider.shape == ColliderShape::Sphere) {
                    // Ray-Sphere intersection (uses world position and scaled radius)
                    float scaleX = glm::length(glm::vec3(worldM[0]));
                    float scaleY = glm::length(glm::vec3(worldM[1]));
                    float scaleZ = glm::length(glm::vec3(worldM[2]));
                    float worldRadius = collider.radius * std::max({scaleX, scaleY, scaleZ});

                    glm::vec3 oc = ray.origin - colPos;
                    float b = glm::dot(oc, ray.direction);
                    float c = glm::dot(oc, oc) - worldRadius * worldRadius;
                    
                    if (c > 0.0f && b > 0.0f) continue;
                    float discriminant = b * b - c;
                    
                    if (discriminant >= 0.0f) {
                        float t = -b - std::sqrt(discriminant);
                        if (t < 0.0f) t = -b + std::sqrt(discriminant);
                        if (t >= 0.0f) {
                            hit.hit = true;
                            hit.distance = t;
                            hit.position = ray.origin + ray.direction * t;
                            hit.normal = glm::normalize(hit.position - colPos);
                        }
                    }
                } else if (collider.shape == ColliderShape::AABB) {
                    // Ray-AABB intersection (Slab method)
                    glm::vec3 aabbMin = colPos - collider.extents;
                    glm::vec3 aabbMax = colPos + collider.extents;

                    float tmin = 0.0f, tmax = std::numeric_limits<float>::max();
                    bool intersect = true;

                    for (int i = 0; i < 3; ++i) {
                        if (std::abs(ray.direction[i]) < 1e-6f) {
                            if (ray.origin[i] < aabbMin[i] || ray.origin[i] > aabbMax[i]) {
                                intersect = false;
                                break;
                            }
                        } else {
                            float invD = 1.0f / ray.direction[i];
                            float t1 = (aabbMin[i] - ray.origin[i]) * invD;
                            float t2 = (aabbMax[i] - ray.origin[i]) * invD;
                            if (t1 > t2) std::swap(t1, t2);
                            tmin = std::max(tmin, t1);
                            tmax = std::min(tmax, t2);
                            if (tmin > tmax) {
                                intersect = false;
                                break;
                            }
                        }
                    }

                    if (intersect && tmax >= 0.0f) {
                        hit.hit = true;
                        hit.distance = tmin;
                        hit.position = ray.origin + ray.direction * tmin;

                        // Calculate AABB face normal at hit point
                        glm::vec3 hitLocal = hit.position - colPos;
                        glm::vec3 normal(0.0f);
                        float minDist = std::numeric_limits<float>::max();
                        for (int i = 0; i < 3; ++i) {
                            float distToMax = std::abs(collider.extents[i] - hitLocal[i]);
                            if (distToMax < minDist) {
                                minDist = distToMax;
                                normal = glm::vec3(0.0f);
                                normal[i] = 1.0f;
                            }
                            float distToMin = std::abs(-collider.extents[i] - hitLocal[i]);
                            if (distToMin < minDist) {
                                minDist = distToMin;
                                normal = glm::vec3(0.0f);
                                normal[i] = -1.0f;
                            }
                        }
                        hit.normal = normal;
                    }
                } else if (collider.shape == ColliderShape::OBB) {
                    // Ray-OBB intersection: transform ray to local space, do Slab check
                    glm::mat4 invM = glm::inverse(worldM);
                    glm::vec3 localOrigin = glm::vec3(invM * glm::vec4(ray.origin, 1.0f));
                    glm::vec3 localDir = glm::normalize(glm::vec3(invM * glm::vec4(ray.direction, 0.0f)));

                    glm::vec3 boxMin = collider.offset - collider.extents;
                    glm::vec3 boxMax = collider.offset + collider.extents;

                    float tmin = 0.0f, tmax = std::numeric_limits<float>::max();
                    bool intersect = true;

                    for (int i = 0; i < 3; ++i) {
                        if (std::abs(localDir[i]) < 1e-6f) {
                            if (localOrigin[i] < boxMin[i] || localOrigin[i] > boxMax[i]) {
                                intersect = false;
                                break;
                            }
                        } else {
                            float invD = 1.0f / localDir[i];
                            float t1 = (boxMin[i] - localOrigin[i]) * invD;
                            float t2 = (boxMax[i] - localOrigin[i]) * invD;
                            if (t1 > t2) std::swap(t1, t2);
                            tmin = std::max(tmin, t1);
                            tmax = std::min(tmax, t2);
                            if (tmin > tmax) {
                                intersect = false;
                                break;
                            }
                        }
                    }

                    if (intersect && tmax >= 0.0f) {
                        glm::vec3 localHitPos = localOrigin + localDir * tmin;
                        glm::vec3 worldHitPos = glm::vec3(worldM * glm::vec4(localHitPos, 1.0f));

                        hit.hit = true;
                        hit.distance = glm::distance(ray.origin, worldHitPos);
                        hit.position = worldHitPos;

                        // Local normal calculation
                        glm::vec3 hitLocal = localHitPos - collider.offset;
                        glm::vec3 localNormal(0.0f);
                        float minDist = std::numeric_limits<float>::max();
                        for (int i = 0; i < 3; ++i) {
                            float distToMax = std::abs(collider.extents[i] - hitLocal[i]);
                            if (distToMax < minDist) {
                                minDist = distToMax;
                                localNormal = glm::vec3(0.0f);
                                localNormal[i] = 1.0f;
                            }
                            float distToMin = std::abs(-collider.extents[i] - hitLocal[i]);
                            if (distToMin < minDist) {
                                minDist = distToMin;
                                localNormal = glm::vec3(0.0f);
                                localNormal[i] = -1.0f;
                            }
                        }
                        
                        // Transform local normal to world space
                        hit.normal = glm::normalize(glm::vec3(worldM * glm::vec4(localNormal, 0.0f)));
                    }
                }

                if (hit.hit && hit.distance < closestHit.distance) {
                    closestHit = hit;
                    closestHit.entity = entity;
                }
            }

            return closestHit;
        }

    private:
        struct OBB {
            glm::vec3 center{0.0f};
            glm::vec3 axes[3]{glm::vec3(1,0,0), glm::vec3(0,1,0), glm::vec3(0,0,1)};
            glm::vec3 extents{0.5f};
        };

        /**
         * @brief Recursively traverses parent references to build the entity's absolute world transform.
         */
        glm::mat4 getEntityWorldMatrix(Entity entity, int depth = 0) {
            if (depth > 100) return glm::mat4(1.0f);
            glm::mat4 m = glm::mat4(1.0f);
            if (auto* t = registry.get<Transform>(entity)) {
                m = t->matrix();
            }
            if (auto* h = registry.get<HierarchyComponent>(entity)) {
                if (h->parent.getId() != Entity::INVALID_ENTITY && registry.isValid(h->parent)) {
                    m = getEntityWorldMatrix(h->parent, depth + 1) * m;
                }
            }
            return m;
        }

        /**
         * @brief Builds OBB structural parameters from transform properties and collider bounds.
         */
        OBB getOBB(Entity entity, const ColliderComponent& col) {
            glm::mat4 worldM = getEntityWorldMatrix(entity);
            
            OBB obb;
            obb.center = glm::vec3(worldM * glm::vec4(col.offset, 1.0f));
            
            for (int i = 0; i < 3; ++i) {
                glm::vec3 colAxis = glm::vec3(worldM[i]);
                float axisLen = glm::length(colAxis);
                obb.axes[i] = (axisLen > 1e-4f) ? (colAxis / axisLen) : glm::vec3(i == 0, i == 1, i == 2);
                obb.extents[i] = col.extents[i] * (axisLen > 1e-4f ? axisLen : 1.0f);
            }
            return obb;
        }

        /**
         * @brief Wraps an AABB with an OBB shell pointing along identity axes.
         */
        OBB getOBBFromAABB(const ColliderComponent& col, const glm::vec3& pos) {
            OBB obb;
            obb.center = pos;
            obb.axes[0] = glm::vec3(1.0f, 0.0f, 0.0f);
            obb.axes[1] = glm::vec3(0.0f, 1.0f, 0.0f);
            obb.axes[2] = glm::vec3(0.0f, 0.0f, 1.0f);
            obb.extents = col.extents;
            return obb;
        }

        /**
         * @brief Evaluates intersections between Sphere and AABB shapes.
         */
        CollisionInfo checkSphereAABB(const ColliderComponent& sphere, const glm::vec3& sPos,
                                       const ColliderComponent& aabb, const glm::vec3& aPos,
                                       bool flipNormal) {
            CollisionInfo info;
            glm::vec3 closestPoint = glm::clamp(sPos, aPos - aabb.extents, aPos + aabb.extents);
            float dist = glm::distance(sPos, closestPoint);

            if (dist < sphere.radius) {
                info.collided = true;
                info.penetration = sphere.radius - dist;

                glm::vec3 dir = sPos - closestPoint;
                float dirLen = glm::length(dir);
                if (dirLen > 1e-4f) {
                    info.normal = glm::normalize(dir);
                } else {
                    glm::vec3 offset = sPos - aPos;
                    float minOverlap = std::numeric_limits<float>::max();
                    glm::vec3 pushNormal(0.0f, 1.0f, 0.0f);

                    for (int i = 0; i < 3; ++i) {
                        float overlap = aabb.extents[i] - std::abs(offset[i]);
                        if (overlap < minOverlap) {
                            minOverlap = overlap;
                            pushNormal = glm::vec3(0.0f);
                            pushNormal[i] = (offset[i] > 0.0f) ? 1.0f : -1.0f;
                        }
                    }
                    info.penetration = sphere.radius + minOverlap;
                    info.normal = pushNormal;
                }

                if (flipNormal) {
                    info.normal = -info.normal;
                }
                info.contactPoint = closestPoint;
            }
            return info;
        }

        /**
         * @brief Evaluates intersections between OBB and Sphere shapes.
         */
        CollisionInfo checkOBBSphere(const OBB& obb, const glm::vec3& sphereCenter, float sphereRadius, bool flipNormal) {
            CollisionInfo info;
            
            glm::vec3 relCenter = sphereCenter - obb.center;
            glm::vec3 localSpherePos(
                glm::dot(relCenter, obb.axes[0]),
                glm::dot(relCenter, obb.axes[1]),
                glm::dot(relCenter, obb.axes[2])
            );
            
            glm::vec3 localClosestPoint = glm::clamp(localSpherePos, -obb.extents, obb.extents);
            
            glm::vec3 worldClosestPoint = obb.center +
                localClosestPoint.x * obb.axes[0] +
                localClosestPoint.y * obb.axes[1] +
                localClosestPoint.z * obb.axes[2];
                
            float dist = glm::distance(sphereCenter, worldClosestPoint);
            
            if (dist < sphereRadius) {
                info.collided = true;
                info.penetration = sphereRadius - dist;
                
                glm::vec3 dir = sphereCenter - worldClosestPoint;
                float dirLen = glm::length(dir);
                if (dirLen > 1e-4f) {
                    info.normal = dir / dirLen;
                } else {
                    float minOverlap = std::numeric_limits<float>::max();
                    glm::vec3 localPushNormal(0.0f, 1.0f, 0.0f);
                    
                    for (int i = 0; i < 3; ++i) {
                        float overlap = obb.extents[i] - std::abs(localSpherePos[i]);
                        if (overlap < minOverlap) {
                            minOverlap = overlap;
                            localPushNormal = glm::vec3(0.0f);
                            localPushNormal[i] = (localSpherePos[i] > 0.0f) ? 1.0f : -1.0f;
                        }
                    }
                    info.penetration = sphereRadius + minOverlap;
                    info.normal = glm::normalize(obb.center +
                        localPushNormal.x * obb.axes[0] +
                        localPushNormal.y * obb.axes[1] +
                        localPushNormal.z * obb.axes[2] - obb.center);
                }
                
                if (flipNormal) {
                    info.normal = -info.normal;
                }
                info.contactPoint = worldClosestPoint;
            }
            
            return info;
        }

        /**
         * @brief Evaluates intersections between two OBBs using the Separating Axis Theorem (SAT).
         */
        CollisionInfo checkOBBOBB(const OBB& obbA, const OBB& obbB) {
            CollisionInfo info;
            
            glm::vec3 T = obbB.center - obbA.center;
            
            glm::vec3 candidateAxes[15];
            int axisCount = 0;
            
            for (int i = 0; i < 3; ++i) {
                candidateAxes[axisCount++] = obbA.axes[i];
            }
            for (int i = 0; i < 3; ++i) {
                candidateAxes[axisCount++] = obbB.axes[i];
            }
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    glm::vec3 axis = glm::cross(obbA.axes[i], obbB.axes[j]);
                    float axisLen = glm::length(axis);
                    if (axisLen > 1e-4f) {
                        candidateAxes[axisCount++] = axis / axisLen;
                    }
                }
            }
            
            float minOverlap = std::numeric_limits<float>::max();
            glm::vec3 bestAxis(0.0f);
            
            for (int i = 0; i < axisCount; ++i) {
                glm::vec3 L = candidateAxes[i];
                
                float d = std::abs(glm::dot(T, L));
                
                float rA = obbA.extents.x * std::abs(glm::dot(obbA.axes[0], L)) +
                           obbA.extents.y * std::abs(glm::dot(obbA.axes[1], L)) +
                           obbA.extents.z * std::abs(glm::dot(obbA.axes[2], L));
                           
                float rB = obbB.extents.x * std::abs(glm::dot(obbB.axes[0], L)) +
                           obbB.extents.y * std::abs(glm::dot(obbB.axes[1], L)) +
                           obbB.extents.z * std::abs(glm::dot(obbB.axes[2], L));
                           
                float overlap = (rA + rB) - d;
                if (overlap <= 0.0f) {
                    return info; // Separating axis found
                }
                
                if (overlap < minOverlap) {
                    minOverlap = overlap;
                    bestAxis = L;
                }
            }
            
            info.collided = true;
            info.penetration = minOverlap;
            
            if (glm::dot(T, bestAxis) < 0.0f) {
                bestAxis = -bestAxis;
            }
            info.normal = bestAxis;
            
            // Compute contact point as the midpoint of support points along bestAxis
            glm::vec3 supportA = obbA.center;
            for (int i = 0; i < 3; ++i) {
                float dotVal = glm::dot(obbA.axes[i], bestAxis);
                supportA += (dotVal >= 0.0f ? 1.0f : -1.0f) * obbA.extents[i] * obbA.axes[i];
            }
            
            glm::vec3 supportB = obbB.center;
            for (int i = 0; i < 3; ++i) {
                float dotVal = glm::dot(obbB.axes[i], -bestAxis);
                supportB += (dotVal >= 0.0f ? 1.0f : -1.0f) * obbB.extents[i] * obbB.axes[i];
            }
            
            info.contactPoint = 0.5f * (supportA + supportB);
            
            return info;
        }

        /**
         * @brief Performs collision checks routing to Sphere-Sphere, AABB-AABB, OBB-Sphere, or OBB-OBB SAT.
         */
        CollisionInfo checkCollision(Entity entityA, const ColliderComponent& colA,
                                     Entity entityB, const ColliderComponent& colB) {
            CollisionInfo info;

            glm::mat4 worldMatrixA = getEntityWorldMatrix(entityA);
            glm::mat4 worldMatrixB = getEntityWorldMatrix(entityB);
            glm::vec3 posA = glm::vec3(worldMatrixA * glm::vec4(colA.offset, 1.0f));
            glm::vec3 posB = glm::vec3(worldMatrixB * glm::vec4(colB.offset, 1.0f));

            if (colA.shape == ColliderShape::Sphere && colB.shape == ColliderShape::Sphere) {
                // Sphere - Sphere
                float rSum = colA.radius + colB.radius;
                float dist = glm::distance(posA, posB);
                if (dist < rSum) {
                    info.collided = true;
                    info.penetration = rSum - dist;
                    glm::vec3 dir = posB - posA;
                    float dirLen = glm::length(dir);
                    info.normal = (dirLen > 1e-4f) ? glm::normalize(dir) : glm::vec3(0.0f, 1.0f, 0.0f);
                    
                    float radiusA = colA.radius;
                    if (dirLen > 1e-4f) {
                        info.contactPoint = posA + info.normal * radiusA;
                    } else {
                        info.contactPoint = 0.5f * (posA + posB);
                    }
                }
            } else if (colA.shape == ColliderShape::Sphere && colB.shape == ColliderShape::AABB) {
                return checkSphereAABB(colA, posA, colB, posB, false);
            } else if (colA.shape == ColliderShape::AABB && colB.shape == ColliderShape::Sphere) {
                return checkSphereAABB(colB, posB, colA, posA, true);
            } else if (colA.shape == ColliderShape::AABB && colB.shape == ColliderShape::AABB) {
                // AABB - AABB (fast path)
                float overlapX = (colA.extents.x + colB.extents.x) - std::abs(posB.x - posA.x);
                float overlapY = (colA.extents.y + colB.extents.y) - std::abs(posB.y - posA.y);
                float overlapZ = (colA.extents.z + colB.extents.z) - std::abs(posB.z - posA.z);
 
                if (overlapX > 0.0f && overlapY > 0.0f && overlapZ > 0.0f) {
                    info.collided = true;
                    if (overlapX < overlapY && overlapX < overlapZ) {
                        info.penetration = overlapX;
                        info.normal = glm::vec3((posB.x > posA.x) ? 1.0f : -1.0f, 0.0f, 0.0f);
                    } else if (overlapY < overlapX && overlapY < overlapZ) {
                        info.penetration = overlapY;
                        info.normal = glm::vec3(0.0f, (posB.y > posA.y) ? 1.0f : -1.0f, 0.0f);
                    } else {
                        info.penetration = overlapZ;
                        info.normal = glm::vec3(0.0f, 0.0f, (posB.z > posA.z) ? 1.0f : -1.0f);
                    }

                    glm::vec3 supportA = posA + info.normal * colA.extents;
                    glm::vec3 supportB = posB - info.normal * colB.extents;
                    info.contactPoint = 0.5f * (supportA + supportB);
                }
            } else if (colA.shape == ColliderShape::Sphere && colB.shape == ColliderShape::OBB) {
                OBB obbB = getOBB(entityB, colB);
                return checkOBBSphere(obbB, posA, colA.radius, true);
            } else if (colA.shape == ColliderShape::OBB && colB.shape == ColliderShape::Sphere) {
                OBB obbA = getOBB(entityA, colA);
                return checkOBBSphere(obbA, posB, colB.radius, false);
            } else {
                // OBB - OBB or OBB - AABB
                OBB obbA = (colA.shape == ColliderShape::OBB) ? getOBB(entityA, colA) : getOBBFromAABB(colA, posA);
                OBB obbB = (colB.shape == ColliderShape::OBB) ? getOBB(entityB, colB) : getOBBFromAABB(colB, posB);
                return checkOBBOBB(obbA, obbB);
            }

            return info;
        }

        /**
         * @brief Separates overlapping transforms and applies impulse response velocities.
         */
        void resolveCollision(Entity entityA, Transform& transA, ColliderComponent& colA,
                              Entity entityB, Transform& transB, ColliderComponent& colB,
                              const CollisionInfo& colInfo) {
            
            RigidBodyComponent* rbA = registry.get<RigidBodyComponent>(entityA);
            RigidBodyComponent* rbB = registry.get<RigidBodyComponent>(entityB);

            bool staticA = !rbA || (rbA->type == RigidBodyType::Static);
            bool staticB = !rbB || (rbB->type == RigidBodyType::Static);

            if (staticA && staticB) return; // Static objects don't resolve

            // 1. Positional Separation (resolves overlap)
            glm::vec3 normal = colInfo.normal;
            float penetration = colInfo.penetration;

            if (staticA) {
                transB.position += normal * penetration;
            } else if (staticB) {
                transA.position -= normal * penetration;
            } else {
                // Both dynamic: distribute based on mass ratio safely
                float totalMass = rbA->mass + rbB->mass;
                float ratioA = 0.5f;
                float ratioB = 0.5f;
                if (totalMass > 1e-4f) {
                    ratioA = rbB->mass / totalMass;
                    ratioB = rbA->mass / totalMass;
                }

                transA.position -= normal * penetration * ratioA;
                transB.position += normal * penetration * ratioB;
            }

            // 2. Velocity & Rotational Resolution (impulse solver)
            if (rbA || rbB) {
                glm::vec3 extentsA = colA.extents;
                if (colA.shape == ColliderShape::OBB) {
                    glm::mat4 worldM = getEntityWorldMatrix(entityA);
                    for (int i = 0; i < 3; ++i) {
                        extentsA[i] *= glm::length(glm::vec3(worldM[i]));
                    }
                } else if (colA.shape == ColliderShape::Sphere) {
                    extentsA = glm::vec3(colA.radius);
                }

                glm::vec3 extentsB = colB.extents;
                if (colB.shape == ColliderShape::OBB) {
                    glm::mat4 worldM = getEntityWorldMatrix(entityB);
                    for (int i = 0; i < 3; ++i) {
                        extentsB[i] *= glm::length(glm::vec3(worldM[i]));
                    }
                } else if (colB.shape == ColliderShape::Sphere) {
                    extentsB = glm::vec3(colB.radius);
                }

                glm::mat3 invInertiaWorldA(0.0f);
                float invMassA = (staticA || rbA->mass < 1e-4f) ? 0.0f : 1.0f / rbA->mass;
                if (!staticA && rbA->mass > 1e-4f) {
                    glm::mat4 worldM = getEntityWorldMatrix(entityA);
                    glm::mat3 R = glm::mat3(worldM);
                    for (int i = 0; i < 3; ++i) {
                        float len = glm::length(R[i]);
                        if (len > 1e-4f) R[i] /= len;
                    }
                    glm::mat3 invInertiaLocal(0.0f);
                    if (colA.shape == ColliderShape::Sphere) {
                        float r = colA.radius;
                        float val = 2.5f / (rbA->mass * r * r);
                        invInertiaLocal = glm::mat3(val);
                    } else {
                        float ex = extentsA.x;
                        float ey = extentsA.y;
                        float ez = extentsA.z;
                        float ix = (1.0f / 3.0f) * rbA->mass * (ey * ey + ez * ez);
                        float iy = (1.0f / 3.0f) * rbA->mass * (ex * ex + ez * ez);
                        float iz = (1.0f / 3.0f) * rbA->mass * (ex * ex + ey * ey);
                        invInertiaLocal[0][0] = 1.0f / ix;
                        invInertiaLocal[1][1] = 1.0f / iy;
                        invInertiaLocal[2][2] = 1.0f / iz;
                    }
                    invInertiaWorldA = R * invInertiaLocal * glm::transpose(R);
                }

                glm::mat3 invInertiaWorldB(0.0f);
                float invMassB = (staticB || rbB->mass < 1e-4f) ? 0.0f : 1.0f / rbB->mass;
                if (!staticB && rbB->mass > 1e-4f) {
                    glm::mat4 worldM = getEntityWorldMatrix(entityB);
                    glm::mat3 R = glm::mat3(worldM);
                    for (int i = 0; i < 3; ++i) {
                        float len = glm::length(R[i]);
                        if (len > 1e-4f) R[i] /= len;
                    }
                    glm::mat3 invInertiaLocal(0.0f);
                    if (colB.shape == ColliderShape::Sphere) {
                        float r = colB.radius;
                        float val = 2.5f / (rbB->mass * r * r);
                        invInertiaLocal = glm::mat3(val);
                    } else {
                        float ex = extentsB.x;
                        float ey = extentsB.y;
                        float ez = extentsB.z;
                        float ix = (1.0f / 3.0f) * rbB->mass * (ey * ey + ez * ez);
                        float iy = (1.0f / 3.0f) * rbB->mass * (ex * ex + ez * ez);
                        float iz = (1.0f / 3.0f) * rbB->mass * (ex * ex + ey * ey);
                        invInertiaLocal[0][0] = 1.0f / ix;
                        invInertiaLocal[1][1] = 1.0f / iy;
                        invInertiaLocal[2][2] = 1.0f / iz;
                    }
                    invInertiaWorldB = R * invInertiaLocal * glm::transpose(R);
                }

                glm::vec3 velA = rbA ? rbA->velocity : glm::vec3(0.0f);
                glm::vec3 velB = rbB ? rbB->velocity : glm::vec3(0.0f);
                glm::vec3 angVelA = rbA ? rbA->angularVelocity : glm::vec3(0.0f);
                glm::vec3 angVelB = rbB ? rbB->angularVelocity : glm::vec3(0.0f);

                glm::vec3 rA = colInfo.contactPoint - transA.position;
                glm::vec3 rB = colInfo.contactPoint - transB.position;

                glm::vec3 contactVelA = velA + glm::cross(angVelA, rA);
                glm::vec3 contactVelB = velB + glm::cross(angVelB, rB);

                glm::vec3 relativeVel = contactVelB - contactVelA;
                float velAlongNormal = glm::dot(relativeVel, normal);

                // Only resolve if they are moving towards each other
                if (velAlongNormal < 0.0f) {
                    float restitutionA = rbA ? rbA->restitution : 0.5f;
                    float restitutionB = rbB ? rbB->restitution : 0.5f;
                    float e = std::min(restitutionA, restitutionB);

                    float impulseScalar = -(1.0f + e) * velAlongNormal;

                    float den = invMassA + invMassB;
                    if (!staticA) {
                        glm::vec3 angComponentA = glm::cross(invInertiaWorldA * glm::cross(rA, normal), rA);
                        den += glm::dot(angComponentA, normal);
                    }
                    if (!staticB) {
                        glm::vec3 angComponentB = glm::cross(invInertiaWorldB * glm::cross(rB, normal), rB);
                        den += glm::dot(angComponentB, normal);
                    }

                    if (den > 1e-4f) {
                        impulseScalar /= den;
                    } else {
                        impulseScalar = 0.0f;
                    }

                    glm::vec3 impulse = impulseScalar * normal;

                    if (!staticA) {
                        rbA->velocity -= invMassA * impulse;
                        rbA->angularVelocity -= invInertiaWorldA * glm::cross(rA, impulse);
                    }
                    if (!staticB) {
                        rbB->velocity += invMassB * impulse;
                        rbB->angularVelocity += invInertiaWorldB * glm::cross(rB, impulse);
                    }
                }
            }

            // Safety guards to instantly clamp and recover from any mathematical NaN/Inf values during collision resolution
            auto validateEntityState = [](Transform& trans, RigidBodyComponent* rb) {
                if (std::isnan(trans.position.x) || std::isinf(trans.position.x) ||
                    std::isnan(trans.position.y) || std::isinf(trans.position.y) ||
                    std::isnan(trans.position.z) || std::isinf(trans.position.z)) {
                    trans.position = glm::vec3(0.0f);
                    if (rb) rb->velocity = glm::vec3(0.0f);
                }
                if (rb) {
                    if (std::isnan(rb->velocity.x) || std::isinf(rb->velocity.x) ||
                        std::isnan(rb->velocity.y) || std::isinf(rb->velocity.y) ||
                        std::isnan(rb->velocity.z) || std::isinf(rb->velocity.z)) {
                        rb->velocity = glm::vec3(0.0f);
                    }
                    if (std::isnan(rb->angularVelocity.x) || std::isinf(rb->angularVelocity.x) ||
                        std::isnan(rb->angularVelocity.y) || std::isinf(rb->angularVelocity.y) ||
                        std::isnan(rb->angularVelocity.z) || std::isinf(rb->angularVelocity.z)) {
                        rb->angularVelocity = glm::vec3(0.0f);
                    }
                }
            };
            validateEntityState(transA, rbA);
            validateEntityState(transB, rbB);
        }

        Registry& registry;
        EditorModeState& editorMode;
    };

} // namespace Engine
