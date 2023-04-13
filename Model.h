#pragma once

#include <string>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Mesh.h"
#include <d3d12.h>
#include "Common.h"
class Model
{
public:
	Model(std::string fileName, ID3D12GraphicsCommandList* commandList,
		Mesh* mesh = nullptr, bool textured = false, bool metalness = false,
		bool ao = false, bool dds = false, string texOverride = "");
	~Model();
	std::vector<Mesh*> mMeshes;
	int mObjConstantBufferIndex = 2;
	int mNumDirtyFrames = 3;

	XMFLOAT4X4 mWorldMatrix = MakeIdentity4x4();

	XMFLOAT3 mPosition = XMFLOAT3{ 0,0,0 };
	XMFLOAT3 mRotation = XMFLOAT3{ 0,0,0 };
	XMFLOAT3 mScale = XMFLOAT3{ 0,0,0 };

	void Draw(ID3D12GraphicsCommandList* commandList);

	void SetPosition(XMFLOAT3 position, bool update = true);
	void SetRotation(XMFLOAT3 rotation, bool update = true);
	void SetScale(XMFLOAT3 scale, bool update = true);

	bool mParallax = false;
	Mesh* mConstructorMesh;
private:
	void ProcessNode(aiNode* node, const aiScene* scene);
	Mesh* ProcessMesh(aiMesh* mesh, const aiScene* scene);
	vector<Texture*> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName, const aiScene* scene);
	int LoadTextureFromFile(const char* path, string directory);
	void LoadEmbeddedTexture(const aiTexture* embeddedTexture);
	void UpdateWorldMatrix();

	std::string mTexOverride;
	std::vector<Texture*> mLoadedTextures;
	std::string mDirectory;
	std::string mFileName;
	std::vector<Texture*> mTextures;
	std::vector<Material*> mMaterials;
	bool mDDS = false;
	bool mMetalness = false;
	bool mAO = false;
	bool mTextured = false;
};

