#pragma once

#include <string>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "GeometryData.h"
#include "DDSTextureLoader.h"

class Model
{
public:
	Model(std::string fileName, ID3D12Device* d3DDevice, ID3D12GraphicsCommandList* commandList);
	~Model();
	GeometryData* mMesh;
	std::vector<GeometryData*> mMeshes;
	int mObjConstantBufferIndex = 2;
	int mNumDirtyFrames = 3;
	XMFLOAT4X4 mWorldMatrix = MakeIdentity4x4();
private:
	void ProcessNode(aiNode* node, const aiScene* scene);
	GeometryData* ProcessMesh(aiMesh* mesh, const aiScene* scene);
	vector<Texture*> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName);
	int LoadTextureFromFile(const char* path, string directory);
	std::vector<Texture*> mLoadedTextures;
	std::string mDirectory;

};

