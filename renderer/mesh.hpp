#pragma once

#include "abstract_renderable.hpp"
#include "buffer.hpp"
#include "hashmap.hpp"
#include "material.hpp"
#include "aabb.hpp"
#include "render_queue.hpp"
#include "limits.hpp"

namespace Granite
{

enum class MeshAttribute : unsigned
{
	Position = 0,
	UV = 1,
	Normal = 2,
	Tangent = 3,
	BoneIndex = 4,
	BoneWeights = 5,
	VertexColor = 6,
	Count
};

enum MeshAttributeFlagBits
{
	MESH_ATTRIBUTE_POSITION_BIT = 1u << Util::ecast(MeshAttribute::Position),
	MESH_ATTRIBUTE_UV_BIT = 1u << Util::ecast(MeshAttribute::UV),
	MESH_ATTRIBUTE_NORMAL_BIT = 1u << Util::ecast(MeshAttribute::Normal),
	MESH_ATTRIBUTE_TANGENT_BIT = 1u << Util::ecast(MeshAttribute::Tangent),
	MESH_ATTRIBUTE_BONE_INDEX_BIT = 1u << Util::ecast(MeshAttribute::BoneIndex),
	MESH_ATTRIBUTE_BONE_WEIGHTS_BIT = 1u << Util::ecast(MeshAttribute::BoneWeights),
	MESH_ATTRIBUTE_VERTEX_COLOR_BIT = 1u << Util::ecast(MeshAttribute::VertexColor)
};

struct MeshAttributeLayout
{
	VkFormat format = VK_FORMAT_UNDEFINED;
	uint32_t offset = 0;
};

struct StaticMeshVertex
{
	mat4 Model;
	mat4 Normal;
	enum
	{
		max_instances = 256
	};
};

struct StaticMeshFragment
{
	vec4 base_color;
	vec4 emissive;
	float roughness;
	float metallic;
	float lod_bias;
};

struct DebugMeshInstanceInfo
{
	vec3 *positions;
	vec4 *colors;
	uint32_t count = 0;
};

struct DebugMeshInfo
{
	Vulkan::Program *program;
	mat4 MVP;
};

struct StaticMeshInstanceInfo
{
	StaticMeshVertex vertex;
};

struct SkinnedMeshInstanceInfo
{
	mat4 *world_transforms = nullptr;
	mat4 *normal_transforms = nullptr;
	uint32_t num_bones = 0;
};

struct StaticMeshInfo
{
	const Vulkan::Buffer *vbo_position;
	const Vulkan::Buffer *vbo_attributes;
	const Vulkan::Buffer *ibo;
	const Vulkan::ImageView *views[Util::ecast(Material::Textures::Count)];
	Vulkan::StockSampler sampler;
	Vulkan::Program *program;
	VkPrimitiveTopology topology;

	MeshAttributeLayout attributes[Util::ecast(MeshAttribute::Count)];

	StaticMeshFragment fragment;

	uint32_t ibo_offset = 0;
	int32_t vertex_offset = 0;
	uint32_t count = 0;

	uint32_t position_stride;
	uint32_t attribute_stride;
	VkIndexType index_type;
	bool two_sided;
	bool alpha_test;
};

namespace RenderFunctions
{
void static_mesh_render(Vulkan::CommandBuffer &cmd, const RenderQueueData *render, unsigned instances);
void debug_mesh_render(Vulkan::CommandBuffer &cmd, const RenderQueueData *render, unsigned instances);
void line_strip_render(Vulkan::CommandBuffer &cmd, const RenderQueueData *render, unsigned instances);
void skinned_mesh_render(Vulkan::CommandBuffer &cmd, const RenderQueueData *render, unsigned instances);
}

struct StaticMesh : AbstractRenderable
{
	Vulkan::BufferHandle vbo_position;
	Vulkan::BufferHandle vbo_attributes;
	Vulkan::BufferHandle ibo;
	uint32_t ibo_offset = 0;
	int32_t vertex_offset = 0;
	uint32_t count = 0;
	uint32_t position_stride = 0;
	uint32_t attribute_stride = 0;
	VkIndexType index_type = VK_INDEX_TYPE_UINT16;
	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	MeshAttributeLayout attributes[Util::ecast(MeshAttribute::Count)];

	MaterialHandle material;

	Util::Hash get_instance_key() const;

	AABB static_aabb;

	void get_render_info(const RenderContext &context, const CachedSpatialTransformComponent *transform,
	                     RenderQueue &queue) const override;

protected:
	void reset();
	void fill_render_info(StaticMeshInfo &info) const;

private:
	bool has_static_aabb() const override
	{
		return true;
	}

	const AABB &get_static_aabb() const override
	{
		return static_aabb;
	}
};

struct SkinnedMesh : public StaticMesh
{
	void get_render_info(const RenderContext &context, const CachedSpatialTransformComponent *transform,
	                     RenderQueue &queue) const override;
};
}
