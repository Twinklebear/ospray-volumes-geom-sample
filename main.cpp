#include <iostream>
#include <vector>
#include <ospray/ospray.h>
#include <ospray/ospcommon/vec.h>
// We use STB image write to write the final render out
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace ospcommon;

void setup_volume(OSPVolume volume, OSPTransferFunction transfer_fcn, const vec3i &volume_dims);

int main(int argc, const char **argv) {
	ospInit(&argc, argv);

	const vec3i volume_dims(256, 256, 256);
	const vec2i img_size(1024, 1024);
	const vec3f cam_pos(-248, -62, 60);
	const vec3f cam_dir = vec3f(volume_dims) / 2.f - cam_pos;
	const vec3f cam_up(0, 1, 0);

	// Setup the camera to render from
	OSPCamera camera = ospNewCamera("perspective");
	ospSet1f(camera, "aspect", img_size.x / static_cast<float>(img_size.y));
	ospSetVec3f(camera, "pos", (osp::vec3f&)cam_pos);
	ospSetVec3f(camera, "dir", (osp::vec3f&)cam_dir);
	ospSetVec3f(camera, "up", (osp::vec3f&)cam_up);
	ospCommit(camera);

	// Setup the a transfer function for the volume
	OSPTransferFunction transfer_fcn = ospNewTransferFunction("piecewise_linear");
	OSPVolume volume = ospNewVolume("block_bricked_volume");
	setup_volume(volume, transfer_fcn, volume_dims);

	OSPModel model = ospNewModel();
	ospAddVolume(model, volume);
	ospCommit(model);

	// Create the renderer we'll use to render the image
	OSPRenderer renderer = ospNewRenderer("scivis");
	ospSetObject(renderer, "model", model);
	ospSetObject(renderer, "camera", camera);
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
	const std::vector<float> opacities = {0.01f, 0.05f, 0.01f};
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
		volume_data[i] = i % 255;
	}
	ospSetString(volume, "voxelType", "uchar");
	ospSetVec3i(volume, "dimensions", (osp::vec3i&)dims);
	ospSetObject(volume, "transferFunction", transfer_fcn);
	// This will copy the volume data into OSPRay's volume where it'll re-organize
	// it for better memory access
	ospSetRegion(volume, volume_data.data(), osp::vec3i{0, 0, 0}, (osp::vec3i&)dims);
	ospCommit(volume);
}

