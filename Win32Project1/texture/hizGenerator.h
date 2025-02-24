#ifndef HIZ_GENERATOR_H_
#define HIZ_GENERATOR_H_

#include "texture2d.h"

struct RenderState;
class StaticNode;
class Camera;
class Render;
class Shader;

class HizGenerator {
private:
	RenderState* state;
	StaticNode* boardNode;
	unsigned int fbo;
	int numLevels;
public:
	HizGenerator();
	~HizGenerator();
public:
	void genMipmap(Render* render, Shader* shader, Texture* texDepth);
	void drawDebug(Camera* camera, Render* render, Shader* shader, Texture* texDepth, int level);
	int getMaxLevel() { return numLevels - 1; }
};

#endif
