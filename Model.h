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
	Model(std::string fileName, ID3D12GraphicsCommandList* commandList, Mesh* mesh = nullptr, string texOverride = "");
	~Model();

	std::vector<Mesh*> mMeshes;
	int mObjConstantBufferIndex = 2;
	int mNumDirtyFrames = 3;

	// Transform
	XMFLOAT3 mPosition = XMFLOAT3{ 0,0,0 };
	XMFLOAT3 mRotation = XMFLOAT3{ 0,0,0 };
	XMFLOAT3 mScale = XMFLOAT3{ 0,0,0 };
	XMFLOAT4X4 mWorldMatrix = MakeIdentity4x4();

	// Draw each mesh in the model
	void Draw(ID3D12GraphicsCommandList* commandList);

	// Set transform components
	void SetPosition(XMFLOAT3 position, bool update = true);
	void SetRotation(XMFLOAT3 rotation, bool update = true);
	void SetScale(XMFLOAT3 scale, bool update = true);

	// Mesh passed in the constructor
	Mesh* mConstructorMesh;

	// Flags for sorting into lists per PSO
	bool mTextured = false;
	bool mPerMeshPBR = false;
	bool mModelTextured = false;
	bool mPerMeshTextured = false;
	bool mParallax = true;
private:
	void ProcessNode(aiNode* node, const aiScene* scene);
	Mesh* ProcessMesh(aiMesh* mesh, const aiScene* scene);
	vector<Texture*> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName, const aiScene* scene);
	int LoadTextureFromFile(const char* path, string directory);
	void LoadEmbeddedTexture(const aiTexture* embeddedTexture);
	void UpdateWorldMatrix();
	bool CheckTextureLoaded(Texture* texture);
	
	// Texture override string
	std::string mTexOverride;
	
	// Array of already loaded textures
	std::vector<Texture*> mLoadedTextures;

	// Name of file
	std::string mDirectory;
	std::string mFileName;

	int mMaterialIndex = 0;

	// Types of file to load
	bool mDDS = false;
	bool mJPG = false;
	bool mPNG = false;

};

