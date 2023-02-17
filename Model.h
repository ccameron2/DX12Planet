#pragma once

#include <string>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Mesh.h"
#include "DDSTextureLoader.h"

class Model
{
public:
	Model(std::string fileName, ID3D12Device* d3DDevice, ID3D12GraphicsCommandList* commandList);
	~Model();
	std::vector<Mesh*> mMeshes;
	int mObjConstantBufferIndex = 2;
	int mNumDirtyFrames = 3;
	XMFLOAT4X4 mWorldMatrix = MakeIdentity4x4();
	void Draw(ID3D12GraphicsCommandList* commandList, ID3D12Resource* matCB);
	std::vector<Material*> mBaseMaterials;
private:
	void ProcessNode(aiNode* node, const aiScene* scene);
	Mesh* ProcessMesh(aiMesh* mesh, const aiScene* scene);
	vector<Texture*> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName, const aiScene* scene);
	int LoadTextureFromFile(const char* path, string directory);
	void LoadEmbeddedTexture(const aiTexture* embeddedTexture);
	std::vector<Texture*> mLoadedTextures;
	std::string mDirectory;
	std::string mFileName;

};

