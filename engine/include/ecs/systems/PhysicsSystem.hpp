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
            dt = std::min(dt, 1.0f / 30.0f);

            glm::vec3 gravityVector{0.0f, -9.81f, 0.0f};

            // 1. Integration Pass (Gravity and Force accumulation)
            for (auto [entity, transform, rb] : registry.view<Transform, RigidBodyComponent>()) {
                if (rb.type == RigidBodyType::Static) continue;

                // Reset contact flag every frame BEFORE collision pass runs
                rb.hadContactThisFrame = false;
                rb.unstableContactThisFrame = false;

                // --- Sleeping body handling ---
                if (rb.sleeping) {
                    // Wake if external code directly set velocity / angular velocity
                    // above the sleep threshold (e.g. launch impulse from game code).
                    bool externalVel   = glm::length(rb.velocity)        > 0.08f;
                    bool externalAngVel= glm::length(rb.angularVelocity) > 0.15f;
                    bool externalForce = glm::length(rb.force)           > 0.01f ||
                                        glm::length(rb.torque)           > 0.01f;
                    if (externalVel || externalAngVel || externalForce) {
                        rb.sleeping   = false;  // Wake up
                        rb.sleepTimer = 0.0f;
                        // Fall through to full integration below
                    } else {
                        // Nothing is moving this body — clear accumulated forces
                        // and skip the rest of integration.
                        rb.force  = glm::vec3(0.0f);
                        rb.torque = glm::vec3(0.0f);
                        continue;
                    }
                }

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

                // Apply linear drag damping (exponential for numerical stability)
                rb.velocity *= std::exp(-rb.linearDrag * dt);

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

                float inertiaScale = 0.5f * massVal;
                glm::vec3 angularAccel = rb.torque / (inertiaScale > 1e-4f ? inertiaScale : 1.0f);
                rb.angularVelocity += angularAccel * dt;

                // Apply angular drag damping (exponential for numerical stability)
                rb.angularVelocity *= std::exp(-rb.angularDrag * dt);

                // Update Euler rotation (degrees)
                transform.rotation += glm::degrees(rb.angularVelocity) * dt;

                // Keep rotation angles within clean range to prevent Euler drift
                transform.rotation.x = std::fmod(transform.rotation.x, 360.0f);
                transform.rotation.y = std::fmod(transform.rotation.y, 360.0f);
                transform.rotation.z = std::fmod(transform.rotation.z, 360.0f);

                // Reset forces and torques
                rb.force  = glm::vec3(0.0f);
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

            // Multiple solver passes approximate the iterative contact solver used by engines
            // like Unity/PhysX and Unreal/Chaos. This is what keeps stacks and resting contacts stable.
            const int passes = 6;
            const float solverDt = dt / static_cast<float>(passes);
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
                                colInfo,
                                solverDt
                            );
                        }
                    }
                }
            }

            // 3. Post-collision Sleep / Velocity-Floor Pass
            // Evaluated AFTER gravity integration AND collision impulses.
            // Only snap to zero if the body was actually moving (speed > 1e-3).
            // This prevents a just-spawned body with vel=0 from immediately sleeping
            // and blocking gravity from ever moving it.
            {
                const float sleepLinThresh = 0.08f;   // m/s
                const float sleepAngThresh = 0.15f;   // rad/s  (~8 deg/s)

                for (auto [entity, transform, rb] : registry.view<Transform, RigidBodyComponent>()) {
                    if (rb.type == RigidBodyType::Static) continue;
                    if (rb.sleeping) continue;

                    float linSpeed = glm::length(rb.velocity);
                    float angSpeed = glm::length(rb.angularVelocity);

                    if (linSpeed < sleepLinThresh && angSpeed < sleepAngThresh) {
                        // Only accumulate sleep time if the body is in contact.
                        // Without contact, the body is in free fall and shouldn't sleep.
                        if (rb.hadContactThisFrame && !rb.unstableContactThisFrame) {
                            rb.sleepTimer += dt;
                            if (rb.sleepTimer >= 0.5f) { // Require 0.5 seconds of sustained rest
                                rb.velocity        = glm::vec3(0.0f);
                                rb.angularVelocity = glm::vec3(0.0f);
                                rb.sleeping        = true;
                                rb.sleepTimer      = 0.0f;
                            }
                        } else {
                            rb.sleepTimer = 0.0f;
                        }
                    } else {
                        // Moving — ensure sleeping flag and timer are reset
                        rb.sleeping   = false;
                        rb.sleepTimer = 0.0f;
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
                
                // Penalize edge-edge (cross product) axes slightly (15% bias) to strongly prefer
                // face contact when overlaps are very close. This prevents numerical noise from
                // selecting a diagonal cross-product axis as the collision normal when a box is
                // resting flat on a face.
                float biasedOverlap = overlap;
                if (i >= 6) {
                    biasedOverlap *= 1.15f;
                }
                
                if (biasedOverlap < minOverlap) {
                    minOverlap = biasedOverlap;
                    bestAxis = L;
                }
            }
            
            info.collided = true;
            info.penetration = minOverlap;
            
            if (glm::dot(T, bestAxis) < 0.0f) {
                bestAxis = -bestAxis;
            }
            info.normal = bestAxis;
            
            // Identify which candidate axis was selected to determine if it is a face or edge contact
            int bestAxisIndex = -1;
            for (int i = 0; i < axisCount; ++i) {
                if (glm::length(candidateAxes[i] - bestAxis) < 1e-3f || glm::length(candidateAxes[i] + bestAxis) < 1e-3f) {
                    bestAxisIndex = i;
                    break;
                }
            }

            // Compute support points along bestAxis
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

            if (bestAxisIndex >= 0 && bestAxisIndex < 3) {
                // Axis is a face normal of obbA: corner of obbB is penetrating obbA's face.
                // The contact point is the penetrating corner of B.
                info.contactPoint = supportB;
            } else if (bestAxisIndex >= 3 && bestAxisIndex < 6) {
                // Axis is a face normal of obbB: corner of obbA is penetrating obbB's face.
                // The contact point is the penetrating corner of A.
                info.contactPoint = supportA;
            } else {
                // Axis is a cross-product (edge-edge): contact point is the midpoint of the closest points on the edges.
                info.contactPoint = 0.5f * (supportA + supportB);
            }
            
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
                return checkSphereAABB(colA, posA, colB, posB, true);
            } else if (colA.shape == ColliderShape::AABB && colB.shape == ColliderShape::Sphere) {
                return checkSphereAABB(colB, posB, colA, posA, false);
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
                               const CollisionInfo& colInfo,
                               float dt) {
            
            RigidBodyComponent* rbA = registry.get<RigidBodyComponent>(entityA);
            RigidBodyComponent* rbB = registry.get<RigidBodyComponent>(entityB);

            bool staticA = !rbA || (rbA->type == RigidBodyType::Static);
            bool staticB = !rbB || (rbB->type == RigidBodyType::Static);

            if (staticA && staticB) return;

            glm::mat4 worldMatrixA = getEntityWorldMatrix(entityA);
            glm::mat4 worldMatrixB = getEntityWorldMatrix(entityB);
            glm::vec3 posA = glm::vec3(worldMatrixA * glm::vec4(colA.offset, 1.0f));
            glm::vec3 posB = glm::vec3(worldMatrixB * glm::vec4(colB.offset, 1.0f));

            OBB obbA = (colA.shape == ColliderShape::OBB) ? getOBB(entityA, colA) : getOBBFromAABB(colA, posA);
            OBB obbB = (colB.shape == ColliderShape::OBB) ? getOBB(entityB, colB) : getOBBFromAABB(colB, posB);

            glm::vec3 normal = colInfo.normal;
            if (glm::length(normal) < 1e-5f) return;
            normal = glm::normalize(normal);

            // --- Snap normal to static face axis to prevent lateral OBB sliding artifacts ---
            if (staticA || staticB) {
                const OBB& staticOBB = staticA ? obbA : obbB;
                float maxDot = 0.0f;
                int bestAxisIndex = -1;
                float bestSign = 1.0f;
                for (int i = 0; i < 3; ++i) {
                    float dotVal = glm::dot(normal, staticOBB.axes[i]);
                    if (std::abs(dotVal) > maxDot) {
                        maxDot = std::abs(dotVal);
                        bestAxisIndex = i;
                        bestSign = (dotVal >= 0.0f) ? 1.0f : -1.0f;
                    }
                }
                // If collision normal is closely aligned with one of the static body's face normals (within ~18 degrees),
                // snap it to that face normal to guarantee forces act purely perpendicular to the static face.
                if (maxDot > 0.95f) {
                    normal = bestSign * staticOBB.axes[bestAxisIndex];
                }
            }

            if (!staticA) rbA->hadContactThisFrame = true;
            if (!staticB) rbB->hadContactThisFrame = true;

            // --- Sleep Wakeup & Bailout Guard ---
            bool aSleeping = rbA && rbA->sleeping;
            bool bSleeping = rbB && rbB->sleeping;

            auto wakeIfMoving = [](RigidBodyComponent* rb, RigidBodyComponent* other, bool otherStatic) {
                if (!rb || !rb->sleeping) return;
                if (otherStatic) return;
                if (!other || other->sleeping) return;
                rb->sleeping   = false;
                rb->sleepTimer = 0.0f;
            };
            wakeIfMoving(rbA, rbB, staticB);
            wakeIfMoving(rbB, rbA, staticA);

            aSleeping = rbA && rbA->sleeping;
            bSleeping = rbB && rbB->sleeping;

            bool skipVelocityA = aSleeping && (staticB || bSleeping);
            bool skipVelocityB = bSleeping && (staticA || aSleeping);

            // --- Compute Inertia Tensors ---
            glm::mat3 invInertiaWorldA(0.0f);
            float invMassA = (staticA || rbA->mass < 1e-4f) ? 0.0f : 1.0f / rbA->mass;
            if (!staticA && rbA->mass > 1e-4f) {
                glm::mat3 R = glm::mat3(obbA.axes[0], obbA.axes[1], obbA.axes[2]);
                glm::mat3 invInertiaLocal(0.0f);
                if (colA.shape == ColliderShape::Sphere) {
                    float r = colA.radius;
                    float val = 2.5f / (rbA->mass * r * r);
                    invInertiaLocal = glm::mat3(val);
                } else {
                    float ex = obbA.extents.x;
                    float ey = obbA.extents.y;
                    float ez = obbA.extents.z;
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
                glm::mat3 R = glm::mat3(obbB.axes[0], obbB.axes[1], obbB.axes[2]);
                glm::mat3 invInertiaLocal(0.0f);
                if (colB.shape == ColliderShape::Sphere) {
                    float r = colB.radius;
                    float val = 2.5f / (rbB->mass * r * r);
                    invInertiaLocal = glm::mat3(val);
                } else {
                    float ex = obbB.extents.x;
                    float ey = obbB.extents.y;
                    float ez = obbB.extents.z;
                    float ix = (1.0f / 3.0f) * rbB->mass * (ey * ey + ez * ez);
                    float iy = (1.0f / 3.0f) * rbB->mass * (ex * ex + ez * ez);
                    float iz = (1.0f / 3.0f) * rbB->mass * (ex * ex + ey * ey);
                    invInertiaLocal[0][0] = 1.0f / ix;
                    invInertiaLocal[1][1] = 1.0f / iy;
                    invInertiaLocal[2][2] = 1.0f / iz;
                }
                invInertiaWorldB = R * invInertiaLocal * glm::transpose(R);
            }

            // Helper lambda for effective mass factors along a direction
            auto computeK = [&](float invM_A, float invM_B,
                                const glm::mat3& invI_A, const glm::mat3& invI_B,
                                const glm::vec3& r_A, const glm::vec3& r_B, const glm::vec3& dir) {
                float K = invM_A + invM_B;
                if (invM_A > 0.0f) {
                    K += glm::dot(glm::cross(invI_A * glm::cross(r_A, dir), r_A), dir);
                }
                if (invM_B > 0.0f) {
                    K += glm::dot(glm::cross(invI_B * glm::cross(r_B, dir), r_B), dir);
                }
                return K;
            };

            glm::vec3 rA = colInfo.contactPoint - transA.position;
            glm::vec3 rB = colInfo.contactPoint - transB.position;

            // --- 1. Position Projection (resolves overlap and rotates flat) ---
            float penetration = colInfo.penetration;
            float slop = 0.0001f; // 0.1 mm slop
            float baumgarte = 0.5f; // resolve 50% of overlap per pass
            float J_p = 0.0f;
            if (penetration > slop) {
                float K_pos = computeK(invMassA, invMassB, invInertiaWorldA, invInertiaWorldB, rA, rB, normal);
                if (K_pos > 1e-4f) {
                    J_p = (penetration - slop) * baumgarte / K_pos;
                }
            }

            if (J_p > 0.0f) {
                // Reset sleep timer while active positional resolution (translation or rotation) is happening
                if (rbA) {
                    rbA->sleeping = false;
                    rbA->sleepTimer = 0.0f;
                }
                if (rbB) {
                    rbB->sleeping = false;
                    rbB->sleepTimer = 0.0f;
                }

                glm::vec3 C_p = J_p * normal;
                if (!staticA) {
                    transA.position -= invMassA * C_p;
                    glm::vec3 deltaThetaA = -invInertiaWorldA * glm::cross(rA, C_p);
                    transA.rotation += glm::degrees(deltaThetaA);
                    transA.rotation.x = std::fmod(transA.rotation.x, 360.0f);
                    transA.rotation.y = std::fmod(transA.rotation.y, 360.0f);
                    transA.rotation.z = std::fmod(transA.rotation.z, 360.0f);
                }
                if (!staticB) {
                    transB.position += invMassB * C_p;
                    glm::vec3 deltaThetaB = invInertiaWorldB * glm::cross(rB, C_p);
                    transB.rotation += glm::degrees(deltaThetaB);
                    transB.rotation.x = std::fmod(transB.rotation.x, 360.0f);
                    transB.rotation.y = std::fmod(transB.rotation.y, 360.0f);
                    transB.rotation.z = std::fmod(transB.rotation.z, 360.0f);
                }
                // Recompute contact vectors relative to new transforms
                rA = colInfo.contactPoint - transA.position;
                rB = colInfo.contactPoint - transB.position;
            }

            if (skipVelocityA && skipVelocityB) return;

            // --- 2. Normal Velocity Resolution (impulse solver) ---
            glm::vec3 velA = rbA ? rbA->velocity : glm::vec3(0.0f);
            glm::vec3 velB = rbB ? rbB->velocity : glm::vec3(0.0f);
            glm::vec3 angVelA = rbA ? rbA->angularVelocity : glm::vec3(0.0f);
            glm::vec3 angVelB = rbB ? rbB->angularVelocity : glm::vec3(0.0f);

            glm::vec3 contactVelA = velA + glm::cross(angVelA, rA);
            glm::vec3 contactVelB = velB + glm::cross(angVelB, rB);
            glm::vec3 relativeVel = contactVelB - contactVelA;

            float velAlongNormal = glm::dot(relativeVel, normal);

            float restitutionA = rbA ? rbA->restitution : (rbB ? rbB->restitution : 0.5f);
            float restitutionB = rbB ? rbB->restitution : (rbA ? rbA->restitution : 0.5f);
            float e = std::min(restitutionA, restitutionB);

            // Restitution threshold
            float linVelAlongN = glm::dot(velB - velA, normal);
            if (velAlongNormal > -0.25f || linVelAlongN > -0.25f) {
                e = 0.0f;
            }

            float K_vel = computeK(invMassA, invMassB, invInertiaWorldA, invInertiaWorldB, rA, rB, normal);
            float impulseScalar = 0.0f;
            
            if (K_vel > 1e-4f) {
                if (velAlongNormal < 0.0f) {
                    impulseScalar = -(1.0f + e) * velAlongNormal / K_vel;
                }
            }

            if (impulseScalar < 0.0f) impulseScalar = 0.0f;

            if (impulseScalar > 0.0f) {
                glm::vec3 impulse = impulseScalar * normal;
                if (!staticA && !skipVelocityA) {
                    rbA->velocity -= invMassA * impulse;
                    rbA->angularVelocity -= invInertiaWorldA * glm::cross(rA, impulse);
                }
                if (!staticB && !skipVelocityB) {
                    rbB->velocity += invMassB * impulse;
                    rbB->angularVelocity += invInertiaWorldB * glm::cross(rB, impulse);
                }
            }

            // --- 3. Friction (tangent impulse) ---
            velA = rbA ? rbA->velocity : glm::vec3(0.0f);
            velB = rbB ? rbB->velocity : glm::vec3(0.0f);
            angVelA = rbA ? rbA->angularVelocity : glm::vec3(0.0f);
            angVelB = rbB ? rbB->angularVelocity : glm::vec3(0.0f);

            contactVelA = velA + glm::cross(angVelA, rA);
            contactVelB = velB + glm::cross(angVelB, rB);
            relativeVel = contactVelB - contactVelA;

            glm::vec3 tangent = relativeVel - glm::dot(relativeVel, normal) * normal;
            float tangentLen = glm::length(tangent);
            if (tangentLen > 1e-5f) {
                tangent = glm::normalize(tangent);
                float K_tangent = computeK(invMassA, invMassB, invInertiaWorldA, invInertiaWorldB, rA, rB, tangent);
                float tangentImpulseScalar = 0.0f;
                if (K_tangent > 1e-4f) {
                    tangentImpulseScalar = -tangentLen / K_tangent;
                }

                float frictionA = rbA ? rbA->friction : (rbB ? rbB->friction : 0.3f);
                float frictionB = rbB ? rbB->friction : (rbA ? rbA->friction : 0.3f);
                float mu = std::sqrt(frictionA * frictionB);
                
                float effectiveNormalImpulse = impulseScalar;
                if (effectiveNormalImpulse < 1e-5f && penetration > 1e-5f) {
                    float massA = (rbA && !staticA) ? rbA->mass : 0.0f;
                    float massB = (rbB && !staticB) ? rbB->mass : 0.0f;
                    float activeMass = 1.0f;
                    if (massA > 0.0f && massB > 0.0f) {
                        activeMass = std::min(massA, massB);
                    } else {
                        activeMass = (massA > 0.0f) ? massA : ((massB > 0.0f) ? massB : 1.0f);
                    }
                    effectiveNormalImpulse = activeMass * 9.81f * dt;
                }
                float maxFriction = mu * effectiveNormalImpulse;

                if (std::abs(tangentImpulseScalar) > maxFriction) {
                    tangentImpulseScalar = (tangentImpulseScalar > 0.0f) ? maxFriction : -maxFriction;
                }

                glm::vec3 tangentImpulse = tangentImpulseScalar * tangent;

                if (!staticA && !skipVelocityA) {
                    rbA->velocity -= invMassA * tangentImpulse;
                    rbA->angularVelocity -= invInertiaWorldA * glm::cross(rA, tangentImpulse);
                }
                if (!staticB && !skipVelocityB) {
                    rbB->velocity += invMassB * tangentImpulse;
                    rbB->angularVelocity += invInertiaWorldB * glm::cross(rB, tangentImpulse);
                }
            }

            // --- 4. Torsional Friction ---
            velA = rbA ? rbA->velocity : glm::vec3(0.0f);
            velB = rbB ? rbB->velocity : glm::vec3(0.0f);
            angVelA = rbA ? rbA->angularVelocity : glm::vec3(0.0f);
            angVelB = rbB ? rbB->angularVelocity : glm::vec3(0.0f);

            glm::vec3 relAngVel = angVelB - angVelA;
            float angVelAlongN = glm::dot(relAngVel, normal);
            if (std::abs(angVelAlongN) > 1e-5f) {
                float K_rot = 0.0f;
                if (!staticA) K_rot += glm::dot(invInertiaWorldA * normal, normal);
                if (!staticB) K_rot += glm::dot(invInertiaWorldB * normal, normal);

                if (K_rot > 1e-5f) {
                    float rotImpulseScalar = -angVelAlongN / K_rot;
                    float frictionA = rbA ? rbA->friction : (rbB ? rbB->friction : 0.3f);
                    float frictionB = rbB ? rbB->friction : (rbA ? rbA->friction : 0.3f);
                    float mu = std::sqrt(frictionA * frictionB);
                    float muRot = mu * 0.1f;
                    
                    float effectiveNormalImpulse = impulseScalar;
                    if (effectiveNormalImpulse < 1e-5f && penetration > 1e-5f) {
                        float massA = (rbA && !staticA) ? rbA->mass : 0.0f;
                        float massB = (rbB && !staticB) ? rbB->mass : 0.0f;
                        float activeMass = 1.0f;
                        if (massA > 0.0f && massB > 0.0f) {
                            activeMass = std::min(massA, massB);
                        } else {
                            activeMass = (massA > 0.0f) ? massA : ((massB > 0.0f) ? massB : 1.0f);
                        }
                        effectiveNormalImpulse = activeMass * 9.81f * dt;
                    }
                    float maxRotFriction = muRot * effectiveNormalImpulse;

                    if (std::abs(rotImpulseScalar) > maxRotFriction) {
                        rotImpulseScalar = (rotImpulseScalar > 0.0f) ? maxRotFriction : -maxRotFriction;
                    }

                    glm::vec3 rotImpulse = rotImpulseScalar * normal;
                    if (!staticA && !skipVelocityA) {
                        rbA->angularVelocity -= invInertiaWorldA * rotImpulse;
                    }
                    if (!staticB && !skipVelocityB) {
                        rbB->angularVelocity += invInertiaWorldB * rotImpulse;
                    }
                }
            }

            static int colDbg = 0;
            bool shouldPrint = (++colDbg % 30 == 0);
            if (shouldPrint) {
                std::cerr << "[Collision] normal=(" << normal.x << "," << normal.y << "," << normal.z
                          << ") pen=" << penetration
                          << " impulseScalar=" << impulseScalar
                          << " velAlongN=" << velAlongNormal
                          << " linVelAlongN=" << linVelAlongN << "\n";
                std::cerr.flush();
                if (!staticB && rbB) {
                    std::cerr << "  -> B_vel=(" << rbB->velocity.x << "," << rbB->velocity.y << "," << rbB->velocity.z
                              << ") B_angVel=(" << rbB->angularVelocity.x << "," << rbB->angularVelocity.y << "," << rbB->angularVelocity.z << ")\n";
                    std::cerr.flush();
                }
            }
        }



        Registry& registry;
        EditorModeState& editorMode;
    };

} // namespace Engine
