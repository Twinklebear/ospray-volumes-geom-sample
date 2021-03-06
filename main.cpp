#include <random>
#include <iostream>
#include <vector>
#include <ospray/ospray.h>
#include <ospray/ospcommon/vec.h>
// We use STB image write to write the final render out
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace ospcommon;

struct Sphere {
	float x, y, z;
	int sphere_id;

	Sphere(float x, float y, float z, int id)
		: x(x), y(y), z(z), sphere_id(id)
	{}
};


void setup_volume(OSPVolume volume, OSPTransferFunction transfer_fcn, const vec3i &volume_dims);
void setup_spheres(OSPGeometry spheres, const vec3i &dims, OSPRenderer renderer);
void setup_streamlines(OSPGeometry streamlines, const vec3i &dims, OSPRenderer renderer);

int main(int argc, const char **argv) {
	ospInit(&argc, argv);

	const vec3i volume_dims(256, 256, 256);
	const vec2i img_size(1024, 1024);
	const vec3f cam_pos(-248, -62, -60);
	const vec3f cam_dir = vec3f(volume_dims) / 2.f - cam_pos;
	const vec3f cam_up(0, 1, 0);

	// Setup the camera to render from
	OSPCamera camera = ospNewCamera("perspective");
	ospSet1f(camera, "aspect", img_size.x / static_cast<float>(img_size.y));
	ospSetVec3f(camera, "pos", (osp::vec3f&)cam_pos);
	ospSetVec3f(camera, "dir", (osp::vec3f&)cam_dir);
	ospSetVec3f(camera, "up", (osp::vec3f&)cam_up);
	ospCommit(camera);

	// Create the renderer we'll use to render the image
	OSPRenderer renderer = ospNewRenderer("scivis");

	// Setup the a transfer function for the volume
	OSPTransferFunction transfer_fcn = ospNewTransferFunction("piecewise_linear");
	OSPVolume volume = ospNewVolume("block_bricked_volume");
	setup_volume(volume, transfer_fcn, volume_dims);

	OSPGeometry spheres = ospNewGeometry("spheres");
	setup_spheres(spheres, volume_dims, renderer);

	OSPGeometry streamlines = ospNewGeometry("streamlines");
	setup_streamlines(streamlines, volume_dims, renderer);

	OSPModel model = ospNewModel();
	ospAddVolume(model, volume);
	ospAddGeometry(model, spheres);
	ospAddGeometry(model, streamlines);
	ospCommit(model);

	// Create an ambient light, which will be used to compute ambient occlusion
	OSPLight ambient_light = ospNewLight(renderer, "ambient");
	ospSet1f(ambient_light, "intensity", 0.2);
	ospCommit(ambient_light);
	OSPLight sun_light = ospNewLight(renderer, "distant");
	ospSetVec3f(sun_light, "direction", osp::vec3f{1.f, 1.f, -0.5f});
	ospSetVec3f(sun_light, "color", osp::vec3f{1.f, 1.f, 0.8f});
	ospSet1f(sun_light, "intensity", 0.8);
	ospSet1f(sun_light, "angularDiameter", 1);
	ospCommit(sun_light);

	OSPLight fill_light = ospNewLight(renderer, "distant");
	ospSetVec3f(fill_light, "direction", osp::vec3f{0.5f, 1.f, 1.5f});
	ospSetVec3f(fill_light, "color", osp::vec3f{1.f, 1.f, 0.8f});
	ospSet1f(fill_light, "intensity", 0.2);
	ospSet1f(fill_light, "angularDiameter", 8);
	ospCommit(fill_light);

	std::vector<OSPLight> lights_list = {ambient_light, sun_light, fill_light};
	OSPData lights = ospNewData(lights_list.size(), OSP_LIGHT, lights_list.data(), 0);
	ospCommit(lights);

	// Setup other renderer params
	ospSetObject(renderer, "model", model);
	ospSetObject(renderer, "camera", camera);
	ospSetObject(renderer, "lights", lights);
	ospSet1i(renderer, "shadowsEnabled", 1);
	ospSet1i(renderer, "aoSamples", 8);
	ospSet1i(renderer, "spp", 16);
	ospSetVec4f(renderer, "bgColor", osp::vec4f{0.01f, 0.01f, 0.01f, 1.f});
	ospCommit(renderer);

	OSPFrameBuffer framebuffer = ospNewFrameBuffer((osp::vec2i&)img_size,
			OSP_FB_SRGBA, OSP_FB_COLOR);
	ospFrameBufferClear(framebuffer, OSP_FB_COLOR);

	// Finally, render the image and save it out
	ospRenderFrame(framebuffer, renderer, OSP_FB_COLOR);

	const uint32_t *fb = static_cast<const uint32_t*>(ospMapFrameBuffer(framebuffer, OSP_FB_COLOR));
	stbi_write_png("sample_render.png", img_size.x, img_size.y, 4, fb, img_size.x * sizeof(uint32_t));
	ospUnmapFrameBuffer(fb, framebuffer);

	return 0;
}
void setup_volume(OSPVolume volume, OSPTransferFunction transfer_fcn, const vec3i &dims) {
	const std::vector<vec3f> colors = {
		vec3f(0, 0, 0.563),
		vec3f(0, 0, 1),
		vec3f(0, 1, 1),
		vec3f(0.5, 1, 0.5),
		vec3f(1, 1, 0),
		vec3f(1, 0, 0),
		vec3f(0.5, 0, 0)
	};
	const std::vector<float> opacities = {0.001f, 0.0025f};
	OSPData colors_data = ospNewData(colors.size(), OSP_FLOAT3, colors.data());
	ospCommit(colors_data);
	OSPData opacity_data = ospNewData(opacities.size(), OSP_FLOAT, opacities.data());
	ospCommit(opacity_data);

	const vec2f value_range(static_cast<float>(0), static_cast<float>(255));
	ospSetData(transfer_fcn, "colors", colors_data);
	ospSetData(transfer_fcn, "opacities", opacity_data);
	ospSetVec2f(transfer_fcn, "valueRange", (osp::vec2f&)value_range);
	ospCommit(transfer_fcn);

	std::vector<unsigned char> volume_data(dims.x * dims.y * dims.z, 0);
	for (size_t i = 0; i < volume_data.size(); ++i) {
		volume_data[i] = (i / (dims.x * dims.y)) * 255;
	}

	// TODO: Here you can also enable gradient shading and volume lighting if you'd like

	ospSetString(volume, "voxelType", "uchar");
	ospSetVec3i(volume, "dimensions", (osp::vec3i&)dims);
	ospSetObject(volume, "transferFunction", transfer_fcn);
	// This will copy the volume data into OSPRay's volume where it'll re-organize
	// it for better memory access
	ospSetRegion(volume, volume_data.data(), osp::vec3i{0, 0, 0}, (osp::vec3i&)dims);
	ospCommit(volume);
}
void setup_spheres(OSPGeometry spheres, const vec3i &dims, OSPRenderer renderer) {
	// See:http://www.ospray.org/documentation.html#spheres
	// Setup our particle data as a sphere geometry.
	// Each particle is an x,y,z center position + a sphere type id, which
	// we'll use to apply different colors for the different sphere types.
	// TODO: Random spheres in the volume dims
	std::vector<Sphere> sphere_vals;
	std::random_device rd;
	std::mt19937 rng(rd());
	std::uniform_real_distribution<float> pos_x(-0.05 * dims.x, 1.05 * dims.x);
	std::uniform_real_distribution<float> pos_y(-0.05 * dims.y, 1.05 * dims.y);
	std::uniform_real_distribution<float> pos_z(-0.05 * dims.z, 1.05 * dims.z);
	for (size_t i = 0; i < 2000; ++i) {
		sphere_vals.push_back(Sphere(pos_x(rng), pos_y(rng), pos_z(rng), i % 3));
	};
	std::vector<float> sphere_colors = {
		1.0, 0.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 0.0, 1.0
	};

	OSPData sphere_data = ospNewData(sphere_vals.size() * sizeof(Sphere), OSP_CHAR,
			sphere_vals.data(), 0);
	ospCommit(sphere_data);
	OSPData color_data = ospNewData(sphere_colors.size(), OSP_FLOAT3,
			sphere_colors.data(), 0);
	ospCommit(color_data);

	// Create the sphere geometry that we'll use to represent our particles
	ospSetData(spheres, "spheres", sphere_data);
	ospSetData(spheres, "color", color_data);
	ospSet1f(spheres, "radius", 3);
	// Tell OSPRay how big each particle is in the spheres array, and where
	// to find the color id. The offset to the center position of the sphere
	// defaults to 0.
	ospSet1f(spheres, "bytes_per_sphere", sizeof(Sphere));
	ospSet1i(spheres, "offset_colorID", 3 * sizeof(float));

	// Set a material for the spheres
	OSPMaterial mat = ospNewMaterial(renderer, "OBJMaterial");
	ospSetVec3f(mat, "Ks", osp::vec3f{0.5, 0.5, 0.5});
	ospSet1f(mat, "Ns", 50.f);
	ospCommit(mat);
	ospSetMaterial(spheres, mat);

	// Our sphere data is now finished being setup, so we commit it to tell
	// OSPRay all the object's parameters are updated.
	ospCommit(spheres);
}
void setup_streamlines(OSPGeometry streamlines, const vec3i &dims, OSPRenderer renderer) {
	// See: http://www.ospray.org/documentation.html#streamlines

	std::random_device rd;
	std::mt19937 rng(rd());
	std::uniform_real_distribution<float> pos_x(-0.05 * dims.x, 1.05 * dims.x);
	std::uniform_real_distribution<float> pos_y(-0.05 * dims.y, 1.05 * dims.y);
	std::uniform_real_distribution<float> pos_z(-0.05 * dims.z, 1.05 * dims.z);
	std::uniform_real_distribution<float> colors(0, 1.0);

	std::vector<vec3fa> vertices;
	std::vector<vec4f> vertex_colors;
	for (size_t i = 0; i < 200; ++i) {
		vertices.push_back(vec3fa(pos_x(rng), pos_y(rng), pos_z(rng)));
		vertex_colors.push_back(vec4f(colors(rng), colors(rng), colors(rng), 1.0));
	};
	// Connecting streamline segments of different lengths and make a total
	// of 20 streamlines
	std::vector<int> indices;
	std::uniform_int_distribution<int> streamline_length(1, 5);
	int next_streamline = 0;
	for (size_t i = 0; i < 20; ++i) {
		const int len = streamline_length(rng);
		for (int j = 0; j < len && next_streamline < vertices.size(); ++j, ++next_streamline) {
			indices.push_back(next_streamline);
		}
		++next_streamline;
	}

	OSPData vertex_data = ospNewData(vertices.size() * sizeof(vec3fa), OSP_FLOAT3A,
			vertices.data(), 0);
	ospCommit(vertex_data);
	OSPData colors_data = ospNewData(vertex_colors.size(), OSP_FLOAT4,
			vertex_colors.data(), 0);
	ospCommit(colors_data);
	OSPData index_data = ospNewData(indices.size(), OSP_INT, indices.data(), 0);
	ospCommit(index_data);

	// The radius is the same for all streamlines in the group, so if you want
	// different ones you should make a separate streamlines geometry
	ospSet1f(streamlines, "radius", 2);
	ospSetData(streamlines, "vertex", vertex_data);
	ospSetData(streamlines, "vertex.color", colors_data);
	ospSetData(streamlines, "index", index_data);

	// Setup a material for the streamlines
	OSPMaterial mat = ospNewMaterial(renderer, "OBJMaterial");
	ospSetVec3f(mat, "Ks", osp::vec3f{0.5, 0.5, 0.5});
	ospSet1f(mat, "Ns", 2.f);
	ospCommit(mat);
	ospSetMaterial(streamlines, mat);

	ospCommit(streamlines);
}

