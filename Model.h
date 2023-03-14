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
	Model(std::string fileName, ID3D12Device* d3DDevice, ID3D12GraphicsCommandList* commandList, Mesh* mesh = nullptr);
	~Model();
	std::vector<Mesh*> mMeshes;
	int mObjConstantBufferIndex = 2;
	int mNumDirtyFrames = 3;

	XMFLOAT4X4 mWorldMatrix = MakeIdentity4x4();

	XMFLOAT3 mPosition = XMFLOAT3{ 0,0,0 };
	XMFLOAT3 mRotation = XMFLOAT3{ 0,0,0 };
	XMFLOAT3 mScale = XMFLOAT3{ 0,0,0 };

	void Draw(ID3D12GraphicsCommandList* commandList, ID3D12Resource* matCB);

	void SetPosition(XMFLOAT3 position, bool update = true);
	void SetRotation(XMFLOAT3 rotation, bool update = true);
	void SetScale(XMFLOAT3 scale, bool update = true);

	std::vector<Material*> mBaseMaterials;
	bool mParallax = false;
private:
	void ProcessNode(aiNode* node, const aiScene* scene);
	Mesh* ProcessMesh(aiMesh* mesh, const aiScene* scene);
	vector<Texture*> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName, const aiScene* scene);
	int LoadTextureFromFile(const char* path, string directory);
	void LoadEmbeddedTexture(const aiTexture* embeddedTexture);
	void UpdateWorldMatrix();

	std::vector<Texture*> mLoadedTextures;
	std::string mDirectory;
	std::string mFileName;

};

