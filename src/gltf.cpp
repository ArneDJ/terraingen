#include <iostream>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <vector>

#include <GL/glew.h>
#include <GL/gl.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include "imp.h"
#include "glwrapper.h"
#include "shader.h"
#include "gltf.h"

static struct bufferobject bind_buffers(std::vector<uint16_t> &indexbuffer, std::vector<vertex> &vertexbuffer)
{
	// https://www.khronos.org/opengl/wiki/Buffer_Object
	// In some cases, data stored in a buffer object will not be changed once it is uploaded. For example, vertex data can be static: set once and used many times.
	// For these cases, you set flags​ to 0 and use data​ as the initial upload. From then on, you simply use the data in the buffer. This requires that you have assembled all of the static data up-front.
	const GLbitfield flags = 0;

	struct bufferobject binding{0};

	//GLuint VAO;
	glGenVertexArrays(1, &binding.VAO);
	glBindVertexArray(binding.VAO);

	//GLuint EBO;
	glGenBuffers(1, &binding.EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, binding.EBO);
	glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16_t)*indexbuffer.size(), indexbuffer.data(), flags);

	//GLuint VBO;
	glGenBuffers(1, &binding.VBO);
	glBindBuffer(GL_ARRAY_BUFFER, binding.VBO);
	glBufferStorage(GL_ARRAY_BUFFER, sizeof(vertex)*vertexbuffer.size(), vertexbuffer.data(), flags);

	// positions
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), BUFFER_OFFSET(offsetof(vertex, position)));
	glEnableVertexAttribArray(0);
	// normals
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), BUFFER_OFFSET(offsetof(vertex, normal)));
	glEnableVertexAttribArray(1);
	// texcoords
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), BUFFER_OFFSET(offsetof(vertex, uv)));
	glEnableVertexAttribArray(2);
	// joints
	glVertexAttribIPointer(3, 4, GL_UNSIGNED_INT, sizeof(vertex), BUFFER_OFFSET(offsetof(vertex, joints)));
	glEnableVertexAttribArray(3);
	// weights
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(vertex), BUFFER_OFFSET(offsetof(vertex, weights)));
	glEnableVertexAttribArray(4);

	//return VAO;
	return binding;
}

static GLuint load_gltf_image(tinygltf::Image &gltfimage)
{
	GLuint texture;

	struct rawimage image {
		.data = &gltfimage.image[0],
		.nchannels = (unsigned int)gltfimage.component,
		.width = size_t(gltfimage.width),
		.height = size_t(gltfimage.height)
	};

	GLenum format = GL_RGBA;
	GLenum internalformat = GL_RGB5_A1;

	texture = bind_mipmap_texture(&image, internalformat, format, GL_UNSIGNED_BYTE);

	return texture;
}

// fills an index buffer and returns the index count
static uint32_t load_indices(const tinygltf::Model &model, const tinygltf::Primitive &primitive, std::vector<uint16_t> &indexbuffer)
{
	uint32_t indexcount = 0;

	const tinygltf::Accessor &accessor = model.accessors[primitive.indices > -1 ? primitive.indices : 0];
	const tinygltf::BufferView &bufview = model.bufferViews[accessor.bufferView];
	const tinygltf::Buffer &buffer = model.buffers[bufview.buffer];

	indexcount = static_cast<uint32_t>(accessor.count);
	const void *data = &(buffer.data[accessor.byteOffset + bufview.byteOffset]);

	switch (accessor.componentType) {
	case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
		std::cerr << "error: UNSIGNED_INT indices not supported\n";
		break;
	}
	case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
		const uint16_t *buf = static_cast<const uint16_t*>(data);
		for (size_t index = 0; index < accessor.count; index++) {
			indexbuffer.push_back(buf[index]);
		}
		break;
	}
	case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
		const uint8_t *buf = static_cast<const uint8_t*>(data);
		for (size_t index = 0; index < accessor.count; index++) {
			indexbuffer.push_back(buf[index]);
		}
		break;
	}
	default:
		std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
	}

	return indexcount;
}

static void draw_primitives(const gltf::primitive_t *prim)
{
	activate_texture(GL_TEXTURE0, GL_TEXTURE_2D, prim->material.basecolormap);
	activate_texture(GL_TEXTURE1, GL_TEXTURE_2D, prim->material.metalroughmap);
	activate_texture(GL_TEXTURE2, GL_TEXTURE_2D, prim->material.normalmap);

	if (prim->indexed == false) {
		glDrawArrays(GL_TRIANGLES, prim->firstvertex, prim->vertexcount);
	} else {
		glDrawElementsBaseVertex(GL_TRIANGLES, prim->indexcount, GL_UNSIGNED_SHORT, (GLvoid *)((prim->firstindex)*sizeof(uint16_t)), prim->firstvertex);
	/* TODO use primitive restart */
	}
}

static void draw_primitives_instanced(const gltf::primitive_t *prim, size_t count)
{
	activate_texture(GL_TEXTURE0, GL_TEXTURE_2D, prim->material.basecolormap);
	activate_texture(GL_TEXTURE1, GL_TEXTURE_2D, prim->material.metalroughmap);
	activate_texture(GL_TEXTURE2, GL_TEXTURE_2D, prim->material.normalmap);

	glDrawElementsInstancedBaseVertex(GL_TRIANGLES, prim->indexcount, GL_UNSIGNED_SHORT, (GLvoid *)((prim->firstindex)*sizeof(uint16_t)), count, prim->firstvertex);
}

void gltf::Model::load_mesh(const tinygltf::Model &model, const tinygltf::Mesh &mesh, gltf::mesh_t *newmesh, std::vector<uint16_t> &indexbuffer, std::vector<vertex> &vertexbuffer)
{
	for (size_t j = 0; j < mesh.primitives.size(); j++) {
		const tinygltf::Primitive &primitive = mesh.primitives[j];
		//uint32_t indexstart = static_cast<uint32_t>(indexbuffer.size());
		uint32_t indexstart = static_cast<uint32_t>(indexbuffer.size());
		uint32_t vertexstart = static_cast<uint32_t>(vertexbuffer.size());
		uint32_t indexcount = 0;
		uint32_t vertexcount = 0;
		bool skinned = false;
		bool indexed = primitive.indices > -1;

		// Indices
		if (indexed) { indexcount = load_indices(model, primitive, indexbuffer); }

		// import vertex data
		const float *bufferpos = nullptr;
		const float *buffernormals = nullptr;
		const float *buffertexcoords = nullptr;
		const uint32_t *bufferjoints = nullptr;
		const float *bufferweights = nullptr;

		int posbytestride = 0;
		int normbytestride = 0;
		int uvbytestride = 0;
		int jointbytestride = 0;
		int weightbytestride = 0;

		// Position attribute is required
		assert(primitive.attributes.find("POSITION") != primitive.attributes.end());

		const tinygltf::Accessor &posaccess = model.accessors[primitive.attributes.find("POSITION")->second];
		const tinygltf::BufferView &posview = model.bufferViews[posaccess.bufferView];
		bufferpos = reinterpret_cast<const float *>(&(model.buffers[posview.buffer].data[posaccess.byteOffset + posview.byteOffset]));
		vertexcount = static_cast<uint32_t>(posaccess.count);
		posbytestride = posaccess.ByteStride(posview) ? (posaccess.ByteStride(posview) / sizeof(float)) : tinygltfsize(TINYGLTF_TYPE_VEC3);


		if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
			const tinygltf::Accessor &normaccess = model.accessors[primitive.attributes.find("NORMAL")->second];
			const tinygltf::BufferView &normview = model.bufferViews[normaccess.bufferView];
			buffernormals = reinterpret_cast<const float *>(&(model.buffers[normview.buffer].data[normaccess.byteOffset + normview.byteOffset]));
			normbytestride = normaccess.ByteStride(normview) ? (normaccess.ByteStride(normview) / sizeof(float)) : tinygltfsize(TINYGLTF_TYPE_VEC3);
		}

		if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
			const tinygltf::Accessor &uvaccess = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
			const tinygltf::BufferView &uvview = model.bufferViews[uvaccess.bufferView];
			buffertexcoords = reinterpret_cast<const float *>(&(model.buffers[uvview.buffer].data[uvaccess.byteOffset + uvview.byteOffset]));
			uvbytestride = uvaccess.ByteStride(uvview) ? (uvaccess.ByteStride(uvview) / sizeof(float)) : tinygltfsize(TINYGLTF_TYPE_VEC2);
		}

		// Joints
		if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end()) {
			const tinygltf::Accessor &jointaccess = model.accessors[primitive.attributes.find("JOINTS_0")->second];
			const tinygltf::BufferView &jointview = model.bufferViews[jointaccess.bufferView];
			bufferjoints = reinterpret_cast<const uint32_t *>(&(model.buffers[jointview.buffer].data[jointaccess.byteOffset + jointview.byteOffset]));
			jointbytestride = jointaccess.ByteStride(jointview) ? (jointaccess.ByteStride(jointview) / sizeof(bufferjoints[0])) : tinygltfsize(TINYGLTF_TYPE_VEC4);
		}

		if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end()) {
			const tinygltf::Accessor &weightaccess = model.accessors[primitive.attributes.find("WEIGHTS_0")->second];
			const tinygltf::BufferView &weightview = model.bufferViews[weightaccess.bufferView];
			bufferweights = reinterpret_cast<const float *>(&(model.buffers[weightview.buffer].data[weightaccess.byteOffset + weightview.byteOffset]));
			weightbytestride = weightaccess.ByteStride(weightview) ? (weightaccess.ByteStride(weightview) / sizeof(float)) : tinygltfsize(TINYGLTF_TYPE_VEC4);
		}

		skinned = (bufferjoints && bufferweights);

		for (size_t v = 0; v < posaccess.count; v++) {
			vertex vert{};
			vert.position = glm::make_vec3(&bufferpos[v * posbytestride]);
			vert.normal = glm::normalize(glm::vec3(buffernormals ? glm::make_vec3(&buffernormals[v * normbytestride]) : glm::vec3(0.0f)));
			vert.uv = buffertexcoords ? glm::make_vec2(&buffertexcoords[v * uvbytestride]) : glm::vec2(0.0f);

			vert.joints = skinned ? glm::ivec4(glm::make_vec4(&bufferjoints[v * jointbytestride])) : glm::ivec4(0.0f);
			vert.weights = skinned ? glm::make_vec4(&bufferweights[v * weightbytestride]) : glm::vec4(0.0f);
			// Fix for all zero weights
			if (glm::length(vert.weights) == 0.0f) { vert.weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); }
			vertexbuffer.push_back(vert);
		}

		struct gltf::primitive_t *newprimitive = new gltf::primitive_t {
			.firstindex = indexstart,
			.indexcount = indexcount,
			.firstvertex = vertexstart,
			.vertexcount = vertexcount,
			.indexed = indexcount > 0,
			.material = primitive.material > -1 ? materials[primitive.material] : materials.back(),
		};

		newmesh->primitives.push_back(newprimitive);
	}
}

void gltf::Model::load_node(gltf::node_t *parent, const tinygltf::Node &node, uint32_t nodeindex, const tinygltf::Model &model, std::vector<uint16_t> &indexbuffer, std::vector<vertex> &vertexbuffer)
{
	gltf::node_t *newnode = new gltf::node_t{};
	newnode->index = nodeindex;
	newnode->parent = parent;
	newnode->name = node.name;
	newnode->skinIndex = node.skin;
	newnode->matrix = glm::mat4(1.0f);

	// get local node matrix
	glm::vec3 translation = glm::vec3(0.0f);
	if (node.translation.size() == 3) {
		translation = glm::make_vec3(node.translation.data());
		newnode->translation = translation;
	}
	glm::mat4 rotation = glm::mat4(1.0f);
	if (node.rotation.size() == 4) {
		glm::quat q = glm::make_quat(node.rotation.data());
		newnode->rotation = glm::mat4(q);
	}
	glm::vec3 scale = glm::vec3(1.0f);
	if (node.scale.size() == 3) {
		scale = glm::make_vec3(node.scale.data());
		newnode->scale = scale;
	}
	if (node.matrix.size() == 16) {
		newnode->matrix = glm::make_mat4x4(node.matrix.data());
	};

	// Node with children
	if (node.children.size() > 0) {
		for (size_t i = 0; i < node.children.size(); i++) {
			load_node(newnode, model.nodes[node.children[i]], node.children[i], model, indexbuffer, vertexbuffer);
		}
	}

	// Node contains mesh data
	if (node.mesh > -1) {
		const tinygltf::Mesh mesh = model.meshes[node.mesh];
		mesh_t *newmesh = new mesh_t(newnode->matrix);
		load_mesh(model, mesh, newmesh, indexbuffer, vertexbuffer);
		newnode->mesh = newmesh;
	}

	if (parent) {
		parent->children.push_back(newnode);
	} else {
		nodes.push_back(newnode);
	}

	linearnodes.push_back(newnode);
}

void gltf::Model::load_animations(tinygltf::Model &gltfModel)
{
	for (tinygltf::Animation &anim : gltfModel.animations) {
		gltf::animation_t animation{};
		animation.name = anim.name;
		if (anim.name.empty()) { animation.name = std::to_string(animations.size()); }

		// Samplers
		for (auto &samp : anim.samplers) {
			gltf::animsampler_t sampler{};

			if (samp.interpolation == "LINEAR") { sampler.interpolation = animsampler_t::interpolationtype::LINEAR; }
			if (samp.interpolation == "STEP") { sampler.interpolation = animsampler_t::interpolationtype::STEP; }
			if (samp.interpolation == "CUBICSPLINE") { sampler.interpolation = animsampler_t::interpolationtype::CUBICSPLINE; }

			// Read sampler input time values
			{
				const tinygltf::Accessor &accessor = gltfModel.accessors[samp.input];
				const tinygltf::BufferView &bufview = gltfModel.bufferViews[accessor.bufferView];
				const tinygltf::Buffer &buffer = gltfModel.buffers[bufview.buffer];

				assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				const void *data = &buffer.data[accessor.byteOffset + bufview.byteOffset];
				const float *buf = static_cast<const float*>(data);
				for (size_t index = 0; index < accessor.count; index++) {
					sampler.inputs.push_back(buf[index]);
				}

				for (auto input : sampler.inputs) {
					if (input < animation.start) { animation.start = input; };
					if (input > animation.end) { animation.end = input; }
				}
			}

			// Read sampler output T/R/S values
			{
				const tinygltf::Accessor &accessor = gltfModel.accessors[samp.output];
				const tinygltf::BufferView &bufview = gltfModel.bufferViews[accessor.bufferView];
				const tinygltf::Buffer &buffer = gltfModel.buffers[bufview.buffer];

				assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				const void *data = &buffer.data[accessor.byteOffset + bufview.byteOffset];

				switch (accessor.type) {
				case TINYGLTF_TYPE_VEC3: {
					const glm::vec3 *buf = static_cast<const glm::vec3*>(data);
					for (size_t index = 0; index < accessor.count; index++) {
						sampler.outputs.push_back(glm::vec4(buf[index], 0.0f));
					}
					break;
				}
				case TINYGLTF_TYPE_VEC4: {
					const glm::vec4 *buf = static_cast<const glm::vec4*>(data);
					for (size_t index = 0; index < accessor.count; index++) {
						sampler.outputs.push_back(buf[index]);
					}
					break;
				}
				default: {
					std::cout << "unknown type" << std::endl;
					break;
				}
				}
			}

			animation.samplers.push_back(sampler);
		}

		// Channels
		for (auto &source: anim.channels) {
			gltf::animchannel_t channel{};

			if (source.target_path == "rotation") {
				channel.path = animchannel_t::pathtype::ROTATION;
			}
			if (source.target_path == "translation") {
				channel.path = animchannel_t::pathtype::TRANSLATION;
			}
			if (source.target_path == "scale") {
				channel.path = animchannel_t::pathtype::SCALE;
			}
			channel.samplerindex = source.sampler;
			channel.target = nodefrom(source.target_node);
			if (!channel.target) { continue; }

			animation.channels.push_back(channel);
		}

		animations.push_back(animation);
	}
}

void gltf::Model::load_skins(tinygltf::Model &gltfModel)
{
	for (tinygltf::Skin &source : gltfModel.skins) {
		gltf::skin_t *newskin = new skin_t{};
		newskin->name = source.name;

		// Find skeleton root node
		if (source.skeleton > -1) {
			newskin->skeletonRoot = nodefrom(source.skeleton);
		}

		// Find joint nodes
		for (int jointindex : source.joints) {
			node_t *node = nodefrom(jointindex);
			if (node) {
				newskin->joints.push_back(nodefrom(jointindex));
			}
		}

		// Get inverse bind matrices from buffer
		if (source.inverseBindMatrices > -1) {
			const tinygltf::Accessor &accessor = gltfModel.accessors[source.inverseBindMatrices];
			const tinygltf::BufferView &bufview = gltfModel.bufferViews[accessor.bufferView];
			const tinygltf::Buffer &buffer = gltfModel.buffers[bufview.buffer];
			newskin->inversebinds.resize(accessor.count);
			memcpy(newskin->inversebinds.data(), &buffer.data[accessor.byteOffset + bufview.byteOffset], accessor.count * sizeof(glm::mat4));
		}

		skins.push_back(newskin);
	}
}

void gltf::Model::load_textures(tinygltf::Model &gltfmodel)
{
	for (tinygltf::Texture &tex : gltfmodel.textures) {
		tinygltf::Image image = gltfmodel.images[tex.source];
		GLuint texture;
		texture = load_gltf_image(image);
		textures.push_back(texture);
	}
}

void gltf::Model::load_materials(tinygltf::Model &gltfmodel)
{
	for (tinygltf::Material &mat : gltfmodel.materials) {
		gltf::material_t material{};

		if (mat.values.find("baseColorTexture") != mat.values.end()) {
			material.basecolormap = textures[mat.values["baseColorTexture"].TextureIndex()];
		}
		if (mat.values.find("metallicRoughnessTexture") != mat.values.end()) {
			material.metalroughmap = textures[mat.values["metallicRoughnessTexture"].TextureIndex()];
		}
		if (mat.values.find("baseColorFactor") != mat.values.end()) {
			material.basecolor = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
		}
		if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end()) {
			material.normalmap = textures[mat.additionalValues["normalTexture"].TextureIndex()];
		}
		if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end()) {
			material.occlusionmap = textures[mat.additionalValues["occlusionTexture"].TextureIndex()];
		}

		materials.push_back(material);
	}

	materials.push_back(material_t{});
}

void gltf::Model::importf(std::string fpath)
{
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	bool binary = false;
	size_t extpos = fpath.rfind('.', fpath.length());
	if (extpos != std::string::npos) {
		binary = (fpath.substr(extpos + 1, fpath.length() - extpos) == "glb");
	} else {
		std::cerr << "Could not open glTF file: " << err << std::endl;
		return;
	}

	bool ret = binary ? loader.LoadBinaryFromFile(&model, &err, &warn, fpath.c_str()) : loader.LoadASCIIFromFile(&model, &err, &warn, fpath.c_str());

	std::vector<uint16_t> indexbuffer;
	std::vector<vertex> vertexbuffer;

	if (!warn.empty()) { printf("Warn: %s\n", warn.c_str()); }
	if (!err.empty()) { printf("Err: %s\n", err.c_str()); }

	if (!ret) { 
		std::cerr << "Could not load glTF file: " << err << std::endl;
		return;
	}

	load_textures(model);
	load_materials(model);
	const tinygltf::Scene &scene = model.scenes[model.defaultScene > -1 ? model.defaultScene : 0];
	for (size_t i = 0; i < scene.nodes.size(); i++) {
		const tinygltf::Node node = model.nodes[scene.nodes[i]];
		load_node(nullptr, node, scene.nodes[i], model, indexbuffer, vertexbuffer);
	}

	//VAO = bind_VAO(indexbuffer, vertexbuffer);
	bufferbind = bind_buffers(indexbuffer, vertexbuffer);

	if (model.animations.size() > 0) { load_animations(model); }
	load_skins(model);

	for (auto node : linearnodes) {
		// Assign skins
		if (node->skinIndex > -1) { node->skin = skins[node->skinIndex]; }
		// Initial pose
		if (node->mesh) { node->update(); }
	}
}

void gltf::Model::update_animation(uint32_t index, float time)
{
	if (animations.empty()) {
		std::cout << ".glTF does not contain animation." << std::endl;
		return;
	}
	if (index > static_cast<uint32_t>(animations.size()) - 1) {
		std::cout << "No animation with index " << index << std::endl;
		return;
	}
	animation_t &animation = animations[index];

	bool updated = false;
	for (auto& channel : animation.channels) {
		gltf::animsampler_t &sampler = animation.samplers[channel.samplerindex];
		if (sampler.inputs.size() > sampler.outputs.size()) { continue; }

		for (size_t i = 0; i < sampler.inputs.size() - 1; i++) {
			if ((time >= sampler.inputs[i]) && (time <= sampler.inputs[i + 1])) {
				float u = std::max(0.0f, time - sampler.inputs[i]) / (sampler.inputs[i + 1] - sampler.inputs[i]);
				if (u <= 1.0f) {
					switch (channel.path) {
					case gltf::animchannel_t::pathtype::TRANSLATION: {
					glm::vec4 trans = glm::mix(sampler.outputs[i], sampler.outputs[i + 1], u);
					channel.target->translation = glm::vec3(trans);
					break;
					}
					case gltf::animchannel_t::pathtype::SCALE: {
					glm::vec4 trans = glm::mix(sampler.outputs[i], sampler.outputs[i + 1], u);
					channel.target->scale = glm::vec3(trans);
					break;
					}
					case gltf::animchannel_t::pathtype::ROTATION: {
					glm::quat q1;
					q1.x = sampler.outputs[i].x;
					q1.y = sampler.outputs[i].y;
					q1.z = sampler.outputs[i].z;
					q1.w = sampler.outputs[i].w;
					glm::quat q2;
					q2.x = sampler.outputs[i + 1].x;
					q2.y = sampler.outputs[i + 1].y;
					q2.z = sampler.outputs[i + 1].z;
					q2.w = sampler.outputs[i + 1].w;
					channel.target->rotation = glm::normalize(glm::slerp(q1, q2, u));
					break;
					}
					}
					updated = true;
				}
			}
		}
	}

	if (updated) {
		for (auto &node : nodes) { node->update(); }
	}
}

void gltf::Model::display(Shader *shader, glm::vec3 translation, float scale)
{
	glBindVertexArray(bufferbind.VAO);

	shader->uniform_bool("skinned", !skins.empty());
	shader->uniform_bool("instanced", instanced);

	for (gltf::node_t *node : linearnodes) {
		if (node->mesh) {
			glm::mat4 m = node->getMatrix();
			glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(scale));
			glm::mat4 T = glm::translate(glm::mat4(1.f), translation);
			shader->uniform_mat4("model", T * S * m);
			shader->uniform_array_mat4("u_joint_matrix", node->mesh->uniformblock.jointcount, node->mesh->uniformblock.jointMatrix); 

			for (const gltf::primitive_t *prim : node->mesh->primitives) {
				shader->uniform_vec3("basedcolor", prim->material.basecolor);
				if (instanced == false) {
					draw_primitives(prim);
				} else {
					draw_primitives_instanced(prim, instance_count);
				}
			}
		}
	}
}

void gltf::Model::instance(size_t count)
{
	instance_count = count;
	instanced = true;
	instance_buffer = instance_dynamic_VAO(bufferbind.VAO, count);
}
