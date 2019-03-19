/* Copyright (c) 2017-2019 Hans-Kristian Arntzen
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "application.hpp"
#include "os_filesystem.hpp"
#include "scene.hpp"
#include "device.hpp"
#include "mesh_util.hpp"
#include "renderer.hpp"
#include "render_context.hpp"
#include "render_components.hpp"
#include "global_managers.hpp"
#include "physics_system.hpp"
#include "muglm/matrix_helper.hpp"

using namespace Granite;

struct PhysicsSandboxApplication : Application, EventHandler
{
	PhysicsSandboxApplication()
		: renderer(RendererType::GeneralForward)
	{
		camera.set_position(vec3(0.0f, 2.0f, 8.0f));
		cube = Util::make_handle<CubeMesh>();
		sphere = Util::make_handle<SphereMesh>();
		init_plane();
		init_scene();
		EVENT_MANAGER_REGISTER_LATCH(PhysicsSandboxApplication, on_swapchain_created, on_swapchain_destroyed, Vulkan::SwapchainParameterEvent);
		EVENT_MANAGER_REGISTER(PhysicsSandboxApplication, on_key, KeyboardEvent);
		EVENT_MANAGER_REGISTER(PhysicsSandboxApplication, on_collision, CollisionEvent);
		EVENT_MANAGER_REGISTER(PhysicsSandboxApplication, on_mouse, MouseButtonEvent);
	}

	bool on_mouse(const MouseButtonEvent &e)
	{
		if (e.get_pressed() && e.get_button() == MouseButton::Left)
		{
			auto result = Global::physics()->query_closest_hit_ray(
					camera.get_position(), camera.get_front(), 100.0f);

			if (result.entity)
			{
				auto *cached = result.entity->get_component<RenderInfoComponent>();
				vec4 local_world = inverse(cached->transform->world_transform) * vec4(result.world_pos, 1.0f);
				vec3 local_normal = inverse(mat3(cached->transform->world_transform)) * result.world_normal;
				auto *new_parent = PhysicsSystem::get_scene_node(result.handle);
				auto node = scene.create_node();
				if (new_parent)
					new_parent->add_child(node);
				else
					scene.get_root_node()->add_child(node);

				node->transform.scale = vec3(0.05f);
				node->transform.translation = local_world.xyz() + local_normal * 0.05f;
				node->invalidate_cached_transform();
				scene.create_renderable(sphere, node.get());
			}
		}

		return true;
	}

	bool on_collision(const CollisionEvent &e)
	{
		auto pos = e.get_world_contact();
		auto n = e.get_world_normal();

		LOGI("Pos: %f, %f, %f\n", pos.x, pos.y, pos.z);
		LOGI("N: %f, %f, %f\n", n.x, n.y, n.z);

		return true;
	}

	void on_swapchain_created(const Vulkan::SwapchainParameterEvent &swap)
	{
		camera.set_aspect(swap.get_aspect_ratio());
		camera.set_fovy(0.4f * pi<float>());
		camera.set_depth_range(0.1f, 500.0f);
	}

	void on_swapchain_destroyed(const Vulkan::SwapchainParameterEvent &)
	{
	}

	void init_plane()
	{
		SceneFormats::Mesh mesh;
		mesh.count = 4;

		const vec3 positions[4] = {
			vec3(-1000.0f, 0.0f, -1000.0f),
			vec3(-1000.0f, 0.0f, +1000.0f),
			vec3(+1000.0f, 0.0f, -1000.0f),
			vec3(+1000.0f, 0.0f, +1000.0f),
		};

		const vec2 uvs[4] = {
			vec2(-1000.0f, -1000.0f),
			vec2(-1000.0f, +1000.0f),
			vec2(+1000.0f, -1000.0f),
			vec2(+1000.0f, +1000.0f),
		};

		mesh.positions.resize(sizeof(positions));
		memcpy(mesh.positions.data(), positions, sizeof(positions));
		mesh.attributes.resize(sizeof(uvs));
		memcpy(mesh.attributes.data(), uvs, sizeof(uvs));
		mesh.position_stride = sizeof(vec3);
		mesh.attribute_stride = sizeof(vec2);
		mesh.attribute_layout[Util::ecast(MeshAttribute::Position)].format = VK_FORMAT_R32G32B32_SFLOAT;
		mesh.attribute_layout[Util::ecast(MeshAttribute::UV)].format = VK_FORMAT_R32G32_SFLOAT;
		mesh.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		mesh.has_material = true;
		mesh.material_index = 0;
		mesh.static_aabb = AABB(vec3(-1000.0f, -1.0f, -1000.0f), vec3(+1000.0f, 0.0f, +1000.0f));

		SceneFormats::MaterialInfo info;
		info.pipeline = DrawPipeline::Opaque;
		info.base_color.path = "builtin://textures/checkerboard.png";
		info.bandlimited_pixel = true;
		info.uniform_roughness = 1.0f;
		info.uniform_metallic = 0.0f;
		plane = Util::make_handle<ImportedMesh>(mesh, info);
	}

	void init_scene()
	{
		auto root_node = scene.create_node();
		auto *entity = scene.create_renderable(plane, root_node.get());
		auto *plane = Global::physics()->add_infinite_plane(vec4(0.0f, 1.0f, 0.0f, 0.0f));
		entity->allocate_component<PhysicsComponent>()->handle = plane;
		PhysicsSystem::set_handle_parent(plane, entity);
		scene.set_root_node(root_node);
		context.set_lighting_parameters(&lighting);
	}

	bool on_key(const KeyboardEvent &e)
	{
		if (e.get_key() == Key::Space && e.get_key_state() == KeyState::Pressed)
		{
			auto &handles = scene.get_entity_pool().get_component_group<PhysicsComponent>();
			for (auto &handle : handles)
			{
				auto *h = get_component<PhysicsComponent>(handle);
				if (!PhysicsSystem::get_scene_node(h->handle))
					continue;

				Global::physics()->apply_impulse(h->handle,
				                                 vec3(0.0f, 22.0f, -4.0f),
				                                 vec3(0.2f, 0.0f, 0.0f));
			}
		}
		else if (e.get_key() == Key::R && e.get_key_state() == KeyState::Pressed)
		{
			auto result = Global::physics()->query_closest_hit_ray(
					camera.get_position(), camera.get_front(), 100.0f);

			if (result.entity && PhysicsSystem::get_scene_node(result.handle))
			{
				auto *node = PhysicsSystem::get_scene_node(result.handle);
				if (node && node->get_children().empty())
					Scene::Node::remove_node_from_hierarchy(node);
				scene.destroy_entity(result.entity);
			}
		}
		else if (e.get_key() == Key::O && e.get_key_state() == KeyState::Pressed)
		{
			auto result = Global::physics()->query_closest_hit_ray(
					camera.get_position(), camera.get_front(), 100.0f);

			if (result.entity)
			{
				auto cube_node = scene.create_node();
				cube_node->transform.translation = result.world_pos + vec3(0.0f, 20.0f, 0.0f);
				cube_node->invalidate_cached_transform();
				scene.get_root_node()->add_child(cube_node);
				auto *entity = scene.create_renderable(cube, cube_node.get());
				PhysicsSystem::MaterialInfo info;
				info.mass = 10.0f;
				info.restitution = 0.05f;
				info.angular_damping = 0.3f;
				info.linear_damping = 0.3f;
				auto *cube = Global::physics()->add_cube(cube_node.get(), info);
				entity->allocate_component<PhysicsComponent>()->handle = cube;
				PhysicsSystem::set_handle_parent(cube, entity);
			}
		}
		else if (e.get_key() == Key::P && e.get_key_state() == KeyState::Pressed)
		{
			auto result = Global::physics()->query_closest_hit_ray(
					camera.get_position(), camera.get_front(), 100.0f);

			if (result.entity)
			{
				auto sphere_node = scene.create_node();
				sphere_node->transform.translation = result.world_pos + vec3(0.0f, 20.0f, 0.0f);
				sphere_node->transform.scale = vec3(0.1f);
				sphere_node->invalidate_cached_transform();
				scene.get_root_node()->add_child(sphere_node);
				auto *entity = scene.create_renderable(sphere, sphere_node.get());
				PhysicsSystem::MaterialInfo info;
				info.mass = 2.0f;
				info.restitution = 0.9f;
				info.angular_damping = 0.3f;
				info.linear_damping = 0.3f;
				auto *sphere = Global::physics()->add_sphere(sphere_node.get(), info);
				entity->allocate_component<PhysicsComponent>()->handle = sphere;
				PhysicsSystem::set_handle_parent(sphere, entity);
			}
		}

		return true;
	}

	void render_frame(double frame_time, double) override
	{
		Global::physics()->iterate(frame_time);
		scene.update_cached_transforms();

		lighting.directional.direction = normalize(vec3(1.0f, 0.5f, 1.0f));
		lighting.directional.color = vec3(1.0f, 0.8f, 0.6f);
		renderer.set_mesh_renderer_options_from_lighting(lighting);
		context.set_camera(camera);
		visible.clear();
		scene.gather_visible_opaque_renderables(context.get_visibility_frustum(), visible);

		auto cmd = get_wsi().get_device().request_command_buffer();
		auto rp = get_wsi().get_device().get_swapchain_render_pass(Vulkan::SwapchainRenderPass::Depth);
		rp.clear_color[0].float32[0] = 0.01f;
		rp.clear_color[0].float32[1] = 0.02f;
		rp.clear_color[0].float32[2] = 0.03f;
		cmd->begin_render_pass(rp);

		renderer.begin();
		renderer.push_renderables(context, visible);
		renderer.flush(*cmd, context, 0);

		cmd->end_render_pass();

		get_wsi().get_device().submit(cmd);
	}

	Scene scene;
	AbstractRenderableHandle cube;
	AbstractRenderableHandle sphere;
	AbstractRenderableHandle plane;
	FPSCamera camera;
	RenderContext context;
	LightingParameters lighting;
	VisibilityList visible;
	Renderer renderer;
};

namespace Granite
{
Application *application_create(int, char **)
{
	application_dummy();

	try
	{
		auto *app = new PhysicsSandboxApplication();
		return app;
	}
	catch (const std::exception &e)
	{
		LOGE("application_create() threw exception: %s\n", e.what());
		return nullptr;
	}
}
}