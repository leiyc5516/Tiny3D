#include "processor.h"
#include "meshBuffer.h"
#include "../constants/constants.h"
#include "../render/renderBuffer.h"
#include "../render/render.h"
#include "../render/renderState.h"
#include "../texture/hizGenerator.h"

const uint DispatchIndex = 0;
const uint CountDataIndex = 1;
const uint Processor::IndNormalIndex = 2;
const uint Processor::IndSingleIndex = 3;
const uint Processor::IndBillbdIndex = 4;
const uint Processor::IndAnimatIndex = 5;
const uint Processor::IndTranspIndex = 6;
const uint InputDataIndex = 7;
const uint LodDataIndex = 8;
const uint Processor::OutputNormal = 9;
const uint Processor::OutputSingle = 10;
const uint Processor::OutputBillbd = 11;
const uint Processor::OutputAnimat = 12;
const uint Processor::OutputTransp = 13;

Processor::Processor(const MeshGather* meshs, const MeshBuffer* meshVBs, const ObjectGather* objects) {
	meshDB = meshs;
	objectDB = objects;
	uniforms = meshVBs->uniforms;

	indNormalCount = meshDB->normals->count;
	indSingleCount = meshDB->singles->count;
	indTranspCount = meshDB->transps->count;
	indBillbdCount = meshDB->billbds->count;
	indAnimatCount = meshDB->animats->count;
	maxSub = meshDB->maxSubCount;

	int countChannel = 8;
	int lodInputChannel = 16, lodOutputChannel = 20;
	buffer = new RenderBuffer(14, false);
	buffer->setBufferData(GL_SHADER_STORAGE_BUFFER, DispatchIndex, GL_ONE, sizeof(DispatchIndirect), GL_STREAM_DRAW, NULL);
	buffer->setBufferData(GL_SHADER_STORAGE_BUFFER, CountDataIndex, GL_UNSIGNED_INT, maxSub, countChannel, GL_STREAM_DRAW, NULL);
	buffer->setBufferData(GL_SHADER_STORAGE_BUFFER, Processor::IndNormalIndex, GL_ONE, indNormalCount * sizeof(Indirect), GL_STREAM_DRAW, meshDB->normals->buffer);
	buffer->setBufferData(GL_SHADER_STORAGE_BUFFER, Processor::IndSingleIndex, GL_ONE, indSingleCount * sizeof(Indirect), GL_STREAM_DRAW, meshDB->singles->buffer);
	buffer->setBufferData(GL_SHADER_STORAGE_BUFFER, Processor::IndBillbdIndex, GL_ONE, indBillbdCount * sizeof(Indirect), GL_STREAM_DRAW, meshDB->billbds->buffer);
	buffer->setBufferData(GL_SHADER_STORAGE_BUFFER, Processor::IndAnimatIndex, GL_ONE, indAnimatCount * sizeof(Indirect), GL_STREAM_DRAW, meshDB->animats->buffer);
	buffer->setBufferData(GL_SHADER_STORAGE_BUFFER, Processor::IndTranspIndex, GL_ONE, indTranspCount * sizeof(Indirect), GL_STREAM_DRAW, meshDB->transps->buffer);
	buffer->setBufferData(GL_SHADER_STORAGE_BUFFER, InputDataIndex, GL_FLOAT, objectDB->maxObjectSize, lodInputChannel, GL_DYNAMIC_DRAW, NULL);
	buffer->setBufferData(GL_SHADER_STORAGE_BUFFER, LodDataIndex, GL_FLOAT, objectDB->maxObjectSize, lodOutputChannel, GL_STREAM_DRAW, NULL);
	buffer->setAttribData(GL_SHADER_STORAGE_BUFFER, Processor::OutputNormal, MeshBuffer::OutputSlot, GL_FLOAT, objectDB->maxNormalSize, 4, 4, false, GL_STREAM_DRAW, 1, NULL);
	buffer->setAttribData(GL_SHADER_STORAGE_BUFFER, Processor::OutputSingle, MeshBuffer::OutputSlot, GL_FLOAT, objectDB->maxSingleSize, 4, 4, false, GL_STREAM_DRAW, 1, NULL);
	buffer->setAttribData(GL_SHADER_STORAGE_BUFFER, Processor::OutputBillbd, MeshBuffer::OutputSlot, GL_FLOAT, objectDB->maxBillbdSize, 4, 4, false, GL_STREAM_DRAW, 1, NULL);
	buffer->setAttribData(GL_SHADER_STORAGE_BUFFER, Processor::OutputAnimat, MeshBuffer::OutputSlot, GL_FLOAT, objectDB->maxAnimatSize, 4, 4, false, GL_STREAM_DRAW, 1, NULL);
	buffer->setAttribData(GL_SHADER_STORAGE_BUFFER, Processor::OutputTransp, MeshBuffer::OutputSlot, GL_FLOAT, objectDB->maxTranspSize, 4, 4, false, GL_STREAM_DRAW, 1, NULL);

	inputPushed = false;
}

Processor::~Processor() {
	delete buffer;
}

void Processor::clear(Render* render) {
	static Shader* shader = render->findShader("clearProcessor");

	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, CountDataIndex, 1);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, DispatchIndex, 2);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::IndNormalIndex, 3);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::IndSingleIndex, 4);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::IndBillbdIndex, 5);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::IndAnimatIndex, 6);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::IndTranspIndex, 7);

	render->useShader(shader);
	render->setShaderUVec4(shader, "uMeshCounts", indNormalCount, indSingleCount, indBillbdCount, indAnimatCount);
	render->setShaderUint(shader, "uTransCounts", indTranspCount);
	glDispatchCompute(maxSub, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
}

void Processor::lod(Render* render, const RenderState* state, const LodParam& param) {
	static Shader* shader = render->findShader("lodProcessor");

	if (uniforms) uniforms->setShaderBase(GL_UNIFORM_BUFFER, MeshBuffer::GroupDataIndex, 1);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, DispatchIndex, 2);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, InputDataIndex, 3);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, CountDataIndex, 4);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, LodDataIndex, 5);

	render->useShader(shader);
	render->setShaderMat4(shader, "viewProjectMatrix", param.vpMat); // frustum culling param
	// lod param
	render->setShaderVec2v(shader, "uDist", param.lodDist);
	render->setShaderVec3v(shader, "eyePos", param.eyePos);
	render->setShaderInt(shader, "uShadowPass", param.shadowPass);
	if (!state->isShadowPass()) { // occlusion culling param
		render->useTexture(TEXTURE_2D, 0, param.depthTex);
		render->setShaderMat4(shader, "prevVPMatrix", param.prevMat);
		render->setShaderVec2v(shader, "uSize", param.size);
		render->setShaderVec2v(shader, "uCamParam", param.camParam);
		render->setShaderFloat(shader, "uMaxLevel", param.maxLevel);
	}
	glDispatchCompute(objectDB->inObjectCount, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void Processor::rearrange(Render* render) {
	static Shader* shader = render->findShader("rearrangeProcessor");

	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, CountDataIndex, 1);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::IndNormalIndex, 2);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::IndSingleIndex, 3);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::IndBillbdIndex, 4);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::IndAnimatIndex, 5);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::IndTranspIndex, 6);

	render->useShader(shader);
	render->setShaderUVec4(shader, "uMeshCounts", indNormalCount, indSingleCount, indBillbdCount, indAnimatCount);
	render->setShaderUint(shader, "uTransCounts", indTranspCount);
	glDispatchCompute(maxSub, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void Processor::gather(Render* render) {
	static Shader* shader = render->findShader("gatherProcessor");

	if (uniforms) uniforms->setShaderBase(GL_UNIFORM_BUFFER, MeshBuffer::BoardDataIndex, 1);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, LodDataIndex, 2);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::IndNormalIndex, 3);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::IndSingleIndex, 4);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::IndBillbdIndex, 5);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::IndAnimatIndex, 6);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::IndTranspIndex, 7);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::OutputNormal, 8);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::OutputSingle, 9);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::OutputBillbd, 10);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::OutputAnimat, 11);
	buffer->setShaderBase(GL_SHADER_STORAGE_BUFFER, Processor::OutputTransp, 12);

	render->useShader(shader);
	buffer->useAs(DispatchIndex, GL_DISPATCH_INDIRECT_BUFFER);
	glDispatchComputeIndirect(0);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
}

void Processor::update() {
	inputObjectCount = objectDB->inObjectCount;
	if (inputObjectCount > 0) {
		buffer->updateBufferData(InputDataIndex, objectDB->inObjectCount, objectDB->inObjectBuffer);
		inputPushed = true;
	}
}

void Processor::showLog() {
	printf("Processor status:\n");
	printf("indNormalCount %d\n", indNormalCount);
	printf("indSingleCount %d\n", indSingleCount);
	printf("indTranspCount %d\n", indTranspCount);
	printf("indBillbdCount %d\n", indBillbdCount);
	printf("indAnimatCount %d\n\n", indAnimatCount);
	if (buffer->streamDatas[InputDataIndex]->bufferid == 0) printf("error buffer\n");
}