/********************************************************/
/*          FBX�ǂݍ��ݒ��ڕ���                                                     */
/********************************************************/
#pragma once

#include <string>
#include <vector>
#include<algorithm>
#include<map>
#include <glm/glm.hpp>
#include <fbxsdk.h>

namespace fbx{

typedef void(*LoadFun)(const FbxObject*);

struct ModelBoneWeight {
    uint8_t boneIndex[4];
    glm::vec4 boneWeight;
};


struct ModelVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;

    uint8_t boneIndex[4];
    glm::vec4 boneWeight;

    bool operator == (const ModelVertex& v) const {
        return std::memcmp(this, &v, sizeof(ModelVertex)) == 0;
    }
};

struct ModelMesh {
    const std::vector<ModelVertex>& GetVertexList() {
        return vertexList;
    }
    const std::vector<unsigned short> GetIndexList() {
        return indexList;
    }

    std::string nodeName;
    std::string materialName;

    std::vector<ModelVertex> vertexList;
    std::vector<unsigned short> indexList;

    glm::mat4 invMeshBaseposeMatrix;
    std::vector<std::string> boneNodeNameList;
    std::vector<glm::mat4> invBoneBaseposeMatrixList;

};

struct ModelMaterial
{
    std::string materialName;

    std::string diffuseTextureName;
    std::string normalTextureName;
    std::string specularTextureName;
    std::string falloffTextureName;
    std::string reflectionMapTextureName;
};

struct FbxAnimation {
    FbxScene* fbxSceneAnimation;
    std::map<std::string, int> nodeIdDictionaryAnimation;
    float animationStartFrame;
    float animationEndFrame;
    float GetAnimationStartFrame() const {
        return this->animationStartFrame;
    }

    float GetAnimationEndFrame() const {
        return this->animationEndFrame;
    }
};

class FbxLoader
{
public:
    FbxLoader();
    ~FbxLoader();

    bool Initialize(const char* filepath);
    void Finalize();

public:
    const std::vector<ModelMesh>& GetMeshList() {
        return mMeshList;
    }
    std::vector<ModelMesh>* GetMeshListPtr() {
        return &mMeshList;
    }

    const std::vector<ModelMaterial>& GetMaterialList() {
        return mMaterialList;
    }
    std::vector<ModelMaterial>* GetMaterialListPtr() {
        return &mMaterialList;
    }

    const std::vector<FbxAnimation>& GetAnimationArray() const {
        return mAnimationArray;
    }
    const int GetMaterialId(const std::string& materialName) const {
        if (this->mMaterialIdDictionary.size() > 0) {
            return this->mMaterialIdDictionary.at(materialName);
        }
        return 0;
    }

    void GetMeshMatrix(float frame, int meshId, glm::mat4 *out_matrix, int animNum) const;
    void GetBoneMatrix(float frame, int meshId, glm::mat4 *out_matrixList, int matrixCount, int animNum) const;
    void GetBoneMatrix(float frame, int meshId, glm::mat4 *out_matrixList, glm::mat4 *matrixList, int matrixCount, int animNum) const;

    bool LoadAnimation(const char* filepath);
protected:

    FbxManager* mManagerPtr;
    FbxScene* mScenePtr;

    ModelMesh ParseMesh(FbxMesh* mesh);
    void ParseNode(FbxNode *nodee);
    void ParseMaterialList(FbxMesh *mesh);
    void ParseMaterial(FbxSurfaceMaterial *material);
    std::vector<FbxAnimation> mAnimationArray;

    std::vector<ModelMesh> mMeshList;
    std::vector<ModelMaterial> mMaterialList;
    std::map<std::string, int> mMaterialIdDictionary;
    std::map<std::string, int> mNodeIdDictionary;

    int mMaterialNum;

};

}
