/* date = February 4th 2024 3:57 pm */

#ifndef HORIZONS_H
#define HORIZONS_H

#include "horizons_platform.h"
#include "horizons_math.h"

typedef struct game_material
{
  vec4 Color;
  f32 Metallic;
  f32 Roughness;
} game_material;

typedef struct game_mesh
{
  platform_mesh Mesh;
  vec3 *CollisionPositions;
  s32 CollisionPositionCount;
  game_material *Material;
} game_mesh;

typedef struct game_aabb
{
  vec3 Max;
  vec3 Min;
  vec3 MidPoint;
  vec3 Scale;
} game_aabb;

typedef enum game_mode
{
  GAME_MODE_VIEW,
  GAME_MODE_FIRST_PERSON,
  GAME_MODE_DEV
} game_mode;

#define MAX_COLLIDERS 32
typedef struct game_state
{
  b32 Initialized;
  memory_arena PermArena;
  memory_arena TempArena;
  game_input LastInput;
  
  f32 Time;
  
  game_mode Mode;
  
  vec3 CameraPosition;
  vec3 CameraRotation;
  vec3 CameraFront;
  vec3 CameraUp;
  f32 CameraSpeed;
  f32 CameraColliderSize;
  b32 IsColliding;
  
  f32 AmbientStrength;
  vec3 LightDirection;
  vec3 LightColor;
  
  platform_shader Shader2d;
  platform_shader OutlineShader;
  game_mesh OutlineMesh;
  
  b32 ControllingCharacter;
  
  f32 CubeRot;
  b32 IsRotating;
  game_mesh *CubeMeshes;
  game_mesh *ConeMeshes;
  
  game_aabb Colliders[MAX_COLLIDERS];
  s32 NumColliders;
} game_state;

#endif //HORIZONS_H
