#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#define HLSL
#include "RaytracingHlslCompat.h"

RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0);
ByteAddressBuffer Indices : register(t1, space0);
StructuredBuffer<Vertex> Vertices : register(t2, space0);

ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);
ConstantBuffer<ObjectConstantBuffer> g_cubeCB : register(b1);
ConstantBuffer<PortalSlot> g_portalCB : register(b2);

// Load three 16 bit indices from a byte addressed buffer.
uint3 Load3x16BitIndices(uint offsetBytes)
{
    uint3 indices;

    // ByteAdressBuffer loads must be aligned at a 4 byte boundary.
    // Since we need to read three 16 bit indices: { 0, 1, 2 } 
    // aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
    // we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
    // based on first index's offsetBytes being aligned at the 4 byte boundary or not:
    //  Aligned:     { 0 1 | 2 - }
    //  Not aligned: { - 0 | 1 2 }
    const uint dwordAlignedOffset = offsetBytes & ~3;    
    const uint2 four16BitIndices = Indices.Load2(dwordAlignedOffset);
 
    // Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
    if (dwordAlignedOffset == offsetBytes)
    {
        indices.x = four16BitIndices.x & 0xffff;
        indices.y = (four16BitIndices.x >> 16) & 0xffff;
        indices.z = four16BitIndices.y & 0xffff;
    }
    else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
    {
        indices.x = (four16BitIndices.x >> 16) & 0xffff;
        indices.y = four16BitIndices.y & 0xffff;
        indices.z = (four16BitIndices.y >> 16) & 0xffff;
    }

    return indices;
}

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
    float4 color;
};

// Retrieve hit world position.
float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float3 HitAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0] +
        attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
    float2 xy = index + 0.5f;
    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

    screenPos.y = -screenPos.y;

    float4 world = mul(float4(screenPos, 0, 1), g_sceneCB.projectionToWorld);

    world.xyz /= world.w;
    origin = g_sceneCB.cameraPosition.xyz;
    direction = normalize(world.xyz - origin);
}

// Diffuse lighting calculation.
float4 CalculateDiffuseLighting(float3 hitPosition, float3 normal)
{
    float3 pixelToLight = normalize(g_sceneCB.lightPosition.xyz - hitPosition);

    float fNDotL = dot(pixelToLight, normal);
    if (fNDotL > 0)
    {
        return g_cubeCB.albedo * g_sceneCB.lightDiffuseColor * fNDotL;
    }
    return 0;
}

//Generate primary rays from camera
[shader("raygeneration")]
void MyRaygenShader()
{
    float3 rayDir;
    float3 origin;
    
    GenerateCameraRay(DispatchRaysIndex().xy, origin, rayDir);

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = rayDir;
    ray.TMin = 0.001;
    ray.TMax = 10000.0;
    RayPayload payload = { float4(0, 0, 0, 0) };
    TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

    RenderTarget[DispatchRaysIndex().xy] = payload.color;
}

//default shader for diffuse lighting
[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{

    float3 hitPosition = HitWorldPosition();

    uint indexSizeInBytes = 2;
    uint indicesPerTriangle = 3;
    uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
    uint baseIndex = PrimitiveIndex() * triangleIndexStride;
    const uint3 indices = Load3x16BitIndices(baseIndex);


    float3 vertexNormals[3] = { 
        Vertices[indices[0]].normal, 
        Vertices[indices[1]].normal, 
        Vertices[indices[2]].normal 
    };

    float3 triangleNormal = HitAttribute(vertexNormals, attr);

    float4 diffuseColor = CalculateDiffuseLighting(hitPosition, triangleNormal);
    float4 color = g_sceneCB.lightAmbientColor + diffuseColor;

    payload.color = color;
}

float3 GetNormal(float3 v0, float3 v1, float3 v2)
{
    float3 U = v1 - v0;
    float3 V = v2 - v0;
    float3 normal = { (U.y * V.z) - (U.z * V.y), (U.z * V.x) - (U.x * V.z), (U.x * V.y) - (U.y * V.x) };
    return normal;
}

//portal shader
[shader("closesthit")]
void PortalClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{

    uint indexSizeInBytes = 2;
    uint indicesPerTriangle = 3;
    uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
    uint baseIndex = PrimitiveIndex() * triangleIndexStride;
    const uint3 indices = Load3x16BitIndices(baseIndex);

    float3 vertexNormals[3] = {
    Vertices[indices[0]].normal,
    Vertices[indices[1]].normal,
    Vertices[indices[2]].normal
    };

    float3 hit_position = HitWorldPosition();

    float3 localhit = hit_position - g_cubeCB.origin_position;

    float3 portalposition = g_portalCB.link_position + localhit;

    float3 A = Vertices[indices[0]].position;
    float3 B = Vertices[indices[1]].position;
    float3 C = Vertices[indices[2]].position;
    float3 portal_dir = WorldRayDirection();  //normalize(GetNormal(A, B, C));
    portal_dir.z *= -1; //reverse ray z direction to get reflected view from portal

    RayDesc portal_ray;
    portal_ray.Origin = portalposition;
    portal_ray.Direction = portal_dir;

    portal_ray.TMin = 0.001;
    portal_ray.TMax = 10000.0;
    RayPayload portal_payload = { float4(0, 0, 0, 0) };

    TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, portal_ray, portal_payload);

    float3 triangleNormal = HitAttribute(vertexNormals, attr);

    float4 diffuseColor = CalculateDiffuseLighting(hit_position, triangleNormal);
    float4 color = g_sceneCB.lightAmbientColor + diffuseColor;
    payload.color = portal_payload.color; //WITHOUT DIFFUSE LIGHTING
    //payload.color = color * portal_payload.color; //WITH DIFFUSE LIGHTING
}

//miss shader when no object hit
[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    float4 background = float4(0.952f, 0.729f, 0.784f, 1.0f);
    payload.color = background;
}

#endif // RAYTRACING_HLSL