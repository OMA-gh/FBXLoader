#include "fbx.h"

#include <memory>
#include <iostream>

namespace fbx{
    
//-- FbxLoader Class --//
FbxLoader::FbxLoader() {
    mMaterialNum = 0;
}
FbxLoader::~FbxLoader()
{
    this->mManagerPtr->Destroy();
}

bool FbxLoader::Initialize(const char* filepath) {
    this->mManagerPtr = FbxManager::Create();
    auto io_setting = FbxIOSettings::Create(this->mManagerPtr, IOSROOT);
    this->mManagerPtr->SetIOSettings(io_setting);
    auto imporeter = FbxImporter::Create(this->mManagerPtr, "");
    if (!imporeter->Initialize(filepath, -1, this->mManagerPtr->GetIOSettings())) {
        return false;
    }
    this->mScenePtr = FbxScene::Create(this->mManagerPtr, "myScene");
    imporeter->Import(this->mScenePtr);
    imporeter->Destroy();

    //�|���S���̎O�p�`�����s��
    FbxGeometryConverter geometry_converter(this->mManagerPtr);
    geometry_converter.Triangulate(this->mScenePtr, true);

    //�����ɓo�^���A�m�[�h������m�[�h��ID���擾�ł���悤�ɂ���
    auto node_count = this->mScenePtr->GetNodeCount();
    printf("NodeCount: %d\n", node_count);
    for (int i = 0; i < node_count; i++) {
        auto fbxNode = this->mScenePtr->GetNode(i);
        this->mNodeIdDictionary.insert({ fbxNode->GetName(),i });
    }

    //�}�e���A���̉��
    auto material_count = this->mScenePtr->GetMaterialCount();
    this->mMaterialList.reserve(material_count);
    printf("materialCount: %d\n", material_count);
    for (int i = 0; i < material_count; ++i)
    {
        auto fbx_material = this->mScenePtr->GetMaterial(i);
        ParseMaterial(fbx_material);
    }

    FbxNode *root_node = mScenePtr->GetRootNode();

    if (root_node) {
        ParseNode(root_node);
    }

    this->LoadAnimation("oma_Test.fbx");
    
    return true;
}

//���b�V���ɒu����C���f�b�N�X�̃��X�g��Ԃ�
std::vector<int> GetIndexList(FbxMesh *mesh) {
    auto polygonCount = mesh->GetPolygonCount();

    std::vector<int> indexList;
    indexList.reserve(polygonCount * 3);
    for (int i = 0; i < polygonCount; i++) {
        indexList.push_back(mesh->GetPolygonVertex(i, 0));
        indexList.push_back(mesh->GetPolygonVertex(i, 1));
        indexList.push_back(mesh->GetPolygonVertex(i, 2));
    }
    return indexList;
}

//���_�̈ʒu���W�̃��X�g��Ԃ�
std::vector<glm::vec3> GetPositionList(FbxMesh *mesh, const std::vector<int>& indexList) {
    std::vector<glm::vec3> positionList;
    positionList.reserve(indexList.size());

    for (auto index : indexList) {
        auto controlPoint = mesh->GetControlPointAt(index);
        positionList.push_back(glm::vec3(controlPoint[0], controlPoint[1], controlPoint[2]));
    }
    return positionList;
}

//����̓��b�V���̖@���̃��X�g��Ԃ�
std::vector<glm::vec3> GetNormalList(FbxMesh *mesh, const std::vector<int> &indexList) {
    auto elementCount = mesh->GetElementNormalCount();
    auto element = mesh->GetElementNormal();
    auto mappingMode = element->GetMappingMode();
    auto referenceMode = element->GetReferenceMode();
    const auto& indexArray = element->GetIndexArray();
    const auto& directArray = element->GetDirectArray();

    std::vector<glm::vec3> normalList;
    normalList.reserve(indexList.size());

    //�}�b�s���O���[�h��fbx�t�@�C�����̖@���̃f�[�^���Ⴄ
    if (mappingMode == FbxGeometryElement::eByControlPoint) {
        for (auto index : indexList) {
            auto normalIndex = (referenceMode == FbxGeometryElement::eDirect) ?
                index : indexArray.GetAt(index);
            //���t�@�����X���[�h��eDirect�Ȃ�΁A�@���͂��̂܂ܓ����Ă���B
            //�\���ȊO�A���eIndextoVertex���Ȃ񂩂�������C���f�b�N�X�z��̔ԍ����Q�Ƃ���
            //�@���̃f�[�^������̂ő��̔ԍ������ԂɎ��o��
            auto normal = directArray.GetAt(normalIndex);
            normalList.push_back(glm::vec3(normal[0], normal[1], normal[2]));
        }
    }
    else if (mappingMode == FbxGeometryElement::eByPolygonVertex) {
        auto indexByPolygonVertex = 0;
        auto polygonCount = mesh->GetPolygonCount();
        for (int i = 0; i < polygonCount; i++) {
            auto polygonSize = mesh->GetPolygonSize(i); //����͎O�p�����Ă���̂őS��3���Ǝv��
            for (int j = 0; j < polygonSize; j++) {
                auto normalIndex = (referenceMode == FbxGeometryElement::eDirect)
                    ? indexByPolygonVertex
                    : indexArray.GetAt(indexByPolygonVertex);
                auto normal = directArray.GetAt(normalIndex);
                normalList.push_back(glm::vec3(normal[0], normal[1], normal[2]));
                ++indexByPolygonVertex;
            }
        }
    }
    else {
        printf("unknown mapping mode\n");
        assert(false);
    }
    return normalList;
}

//���b�V�����̃e�N�X�`�����W�����X�g�ɂ��ĕԂ�
std::vector<glm::vec2> GetUVList(FbxMesh *mesh, const std::vector<int>& indexList, int uvNo) {
    std::vector<glm::vec2> uvList;
    auto elementCout = mesh->GetElementUVCount();
    if (uvNo + 1 > elementCout) {
        return uvList;
    }
    auto element = mesh->GetElementUV(uvNo);
    auto mappingMode = element->GetMappingMode();
    auto referenceMode = element->GetReferenceMode();
    const auto& indexArray = element->GetIndexArray();
    const auto& directArray = element->GetDirectArray();

    uvList.reserve(indexList.size());
    if (mappingMode == FbxGeometryElement::eByControlPoint) {
        //�@���Ƃ�����������
        for (auto index : indexList) {
            auto uvIndex = (referenceMode == FbxGeometryElement::eDirect)
                ? index
                : indexArray.GetAt(index);
            auto uv = directArray.GetAt(uvIndex);
            uvList.push_back(glm::vec2(uv[0], uv[1]));
        }
    }
    else if (mappingMode == FbxGeometryElement::eByPolygonVertex) {
        auto indexByPolygonVetex = 0;
        auto polygonCount = mesh->GetPolygonCount();
        for (int i = 0; i < polygonCount; i++) {
            auto polygonSize = mesh->GetPolygonSize(i);
            for (int j = 0; j < polygonSize; j++) {
                auto uvIndex = (referenceMode == FbxGeometryElement::eDirect)
                    ? indexByPolygonVetex
                    : indexArray.GetAt(indexByPolygonVetex);
                auto uv = directArray.GetAt(uvIndex);
                uvList.push_back(glm::vec2(uv[0], uv[1]));
                ++indexByPolygonVetex;
            }
        }
    }
    else {
        printf("unknown mapping mode\n");
        assert(false);
    }
    return uvList;
}

//�E�F�C�g�����b�V�����ƂɎ�ɓ���ĕԂ�
void GetWeight(FbxMesh *mesh, const std::vector<int>& indexList, std::vector<ModelBoneWeight>& boneWeightList, std::vector<std::string> &boneNodeNameList, std::vector<glm::mat4> &invBaseposeMatrixList) {
    auto skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
    if (skinCount == 0) {
        return;
    }
    assert(skinCount <= 1);

    auto controlPointsCount = mesh->GetControlPointsCount();
    using TmpWeight = std::pair<int, float>;
    std::vector<std::vector<TmpWeight>> tmpBoneWeightList(controlPointsCount);

    auto skin = static_cast<FbxSkin*>(mesh->GetDeformer(0, FbxDeformer::eSkin));

    auto clusterCount = skin->GetClusterCount();
    for (int i = 0; i < clusterCount; i++) {
        auto cluster = skin->GetCluster(i);

        assert(cluster->GetLinkMode() == FbxCluster::eNormalize);

        boneNodeNameList.push_back(cluster->GetLink()->GetName());
        //FBXSDK_printf("%s\n",cluster->GetLink()->GetName());

        auto indexCount = cluster->GetControlPointIndicesCount();
        auto indices = cluster->GetControlPointIndices();
        auto weights = cluster->GetControlPointWeights();

        for (int k = 0; k < indexCount; k++) {
            int controlPointIndex = indices[k];
            tmpBoneWeightList[controlPointIndex].push_back({ i, weights[k] });
        }

        glm::mat4 invBaseposeMatrix; //�x�[�X�|�[�Y�̋t�s��A�ǂ����Ŏg������

        auto baseposeMatrix = cluster->GetLink()->EvaluateGlobalTransform().Inverse();
        auto baseposeMatrixPtr = (double*)baseposeMatrix;
        for (int k = 0; k < 16; k++) {
            invBaseposeMatrix[k / 4][k % 4] = (float)baseposeMatrixPtr[k];
        }
        invBaseposeMatrixList.push_back(invBaseposeMatrix);
    }

    std::vector<ModelBoneWeight> boneWeightListControlPoints;
    for (auto& tmpBoneWeight : tmpBoneWeightList) {
        //�E�F�C�g�̑傫���Ń\�[�g
        std::sort(tmpBoneWeight.begin(), tmpBoneWeight.end(), [](const TmpWeight& weightA, const TmpWeight& weightB) { return weightA.second > weightB.second; });
        //Unitychan.fbx�Ƃ��ɂ̓E�F�C�g��6�܂ŗp�ӂ���Ă���ׁAGLSL�Ŏg���₷���悤��4�Ɍ��炷�Ƃ������Ƃ����Ă���
        while (tmpBoneWeight.size() > 4) {
            tmpBoneWeight.pop_back();
        }

        while (tmpBoneWeight.size() < 4) {
            tmpBoneWeight.push_back({ 0, 0.0f }); //�_�~�[�����Ƃ�
        }
        ModelBoneWeight weight;
        float total = 0.0f;
        for (int i = 0; i < 4; i++) {
            weight.boneIndex[i] = tmpBoneWeight[i].first;
            weight.boneWeight[i] = tmpBoneWeight[i].second;
            total += tmpBoneWeight[i].second;
        }
        //���K��
        for (int i = 0; i < 4; i++) {
            weight.boneWeight[i] /= total;
        }
        boneWeightListControlPoints.push_back(weight);
    }

    for (auto index : indexList) {
        boneWeightList.push_back(boneWeightListControlPoints[index]);
    }
}

//�A�j���[�V�����𓾂�B
//**animationStartFrame ->�n�܂�̃t���[��
//**animationEndFrame ->�I���̃t���[��
//**nodeIdDictionaryAnimation ->�A�j���[�V�����̃m�[�h�������^�Ƀ��X�g�ɓ���Ă���
bool FbxLoader::LoadAnimation(const char* filepath)
{
    auto importer = FbxImporter::Create(this->mManagerPtr, "");

    if (!importer->Initialize(filepath, -1, this->mManagerPtr->GetIOSettings())) {
        FBXSDK_printf("Animation Load Error!:%s\n", filepath);
        return false;
    }
    FbxAnimation fbx_animation;

    FBXSDK_printf("-----------------------------------------------\n");
    FBXSDK_printf("loading animation start:%s---------------------\n", filepath);
    fbx_animation.fbxSceneAnimation = FbxScene::Create(this->mManagerPtr, "animationScene");
    importer->Import(fbx_animation.fbxSceneAnimation);

    auto animStackCount = importer->GetAnimStackCount();
    FBXSDK_printf("Name: %s\n", importer->GetName());
    assert(animStackCount > 0);
    for (int i = 0; i < animStackCount; i++) {
        auto takeInfo = importer->GetTakeInfo(i);

        FBXSDK_printf("Name: %s\n", takeInfo->mName);
        auto importOffset = takeInfo->mImportOffset;
        auto startTime = takeInfo->mLocalTimeSpan.GetStart();
        auto stopTime = takeInfo->mLocalTimeSpan.GetStop();

        fbx_animation.animationStartFrame = (importOffset.Get() + startTime.Get()) / (float)FbxTime::GetOneFrameValue(FbxTime::eFrames60);
        fbx_animation.animationEndFrame = (importOffset.Get() + stopTime.Get()) / (float)FbxTime::GetOneFrameValue(FbxTime::eFrames60);

        // �m�[�h������m�[�hID���擾�ł���悤�Ɏ����ɓo�^
        auto nodeCount = fbx_animation.fbxSceneAnimation->GetNodeCount();
        FBXSDK_printf("animationNodeCount: %d\n", nodeCount);
        for (int i = 0; i < nodeCount; ++i)
        {
            auto fbxNode = fbx_animation.fbxSceneAnimation->GetNode(i);
            fbx_animation.nodeIdDictionaryAnimation.insert({ fbxNode->GetName(), i });
        }
        FBXSDK_printf("strtframe %f\n", fbx_animation.GetAnimationStartFrame());
        FBXSDK_printf("endframe %f\n", fbx_animation.GetAnimationEndFrame());
        FBXSDK_printf("-----------------------------------------------\n");
        this->mAnimationArray.push_back(fbx_animation);
    }
    importer->Destroy();
    return true;
}

void FbxLoader::Finalize() {
    this->mMeshList.clear();
    this->mMaterialList.clear();
    this->mMaterialIdDictionary.clear();
    this->mNodeIdDictionary.clear();
}

//�m�[�h������
void FbxLoader::ParseNode(FbxNode *node) {
    int numChildren = node->GetChildCount();
    FbxNode *childNode = 0;
    for (int i = 0; i < numChildren; i++) {
        childNode = node->GetChild(i);
        FbxMesh* mesh = childNode->GetMesh();
        if (mesh != NULL) {
            this->mMeshList.push_back(this->ParseMesh(mesh));
            ParseMaterialList(mesh);
        }
        ParseNode(childNode);
    }
}

//����ꂽ���b�V���𑖍����Ē��_�E�C���f�b�N�X�E�@���E�E�F�C�g�E�{�[���C���f�b�N�X�𓾂ă��X�g�Ƀv�b�V������
ModelMesh FbxLoader::ParseMesh(FbxMesh *mesh) {
    auto node = mesh->GetNode();

    ModelMesh modelMesh;
    modelMesh.nodeName = node->GetName();
    if (fbxsdk::FbxSurfaceMaterial* mt = node->GetMaterial(0)) {
        modelMesh.materialName = mt->GetName();
    }
    else {
        modelMesh.materialName = "";
    }
    printf(">> mesh: %s\n", modelMesh.nodeName.c_str());

    //�x�[�X�|�[�Y�̋t�s��H
    auto baseposeMatrix = node->EvaluateGlobalTransform().Inverse();
    auto baseposeMatrixPtr = (double*)baseposeMatrix;

    for (int i = 0; i < 16; i++) {
        modelMesh.invMeshBaseposeMatrix[i / 4][i % 4] = (float)baseposeMatrixPtr[i];
    }

    //�C���f�b�N�X�擾�t�F�C�Y
    auto indexList = GetIndexList(mesh);

    //���_���擾
    auto positionList = GetPositionList(mesh, indexList);
    auto normalList = GetNormalList(mesh, indexList);
    auto uvList = GetUVList(mesh, indexList, 0);

    std::vector<ModelBoneWeight> boneWeightList;
    GetWeight(mesh, indexList, boneWeightList, modelMesh.boneNodeNameList, modelMesh.invBoneBaseposeMatrixList);

    std::vector<ModelVertex> modelVertexList;
    modelVertexList.reserve(indexList.size());

    for (unsigned int i = 0; i < indexList.size(); i++) {
        ModelVertex vertex;
        vertex.position = positionList[i];
        vertex.normal = normalList[i];
        vertex.uv = (uvList.size() == 0)
            ? glm::vec2(0.0f, 0.0f)
            : uvList[i];
        if (boneWeightList.size() > 0) {
            for (int j = 0; j < 4; ++j) {
                vertex.boneIndex[j] = boneWeightList[i].boneIndex[j];
            }
            vertex.boneWeight = boneWeightList[i].boneWeight;
        }
        else {
            for (int j = 0; j < 4; ++j) {
                vertex.boneIndex[j] = 0;
            }
            vertex.boneWeight = glm::vec4(1, 0, 0, 0);
        }
        modelVertexList.push_back(vertex);
    }

    //glDrawArrays()�ɂ��`�悪�\�ɂȂ�B
    //�C���f�b�N�X�̃^�[��
    //�d�����_������
    auto& modelVertexListOpt = modelMesh.vertexList;
    modelVertexListOpt.reserve(modelVertexList.size());

    auto& modelIndexList = modelMesh.indexList;
    modelIndexList.reserve(indexList.size());

    for (auto& vertex : modelVertexList) {
        //�d�����Ă��邩
        auto it = std::find(modelVertexListOpt.begin(), modelVertexListOpt.end(), vertex);
        if (it == modelVertexListOpt.end()) {
            //it�����X�g�̍Ō���w���Ă���̂ŁA�d�����Ă��Ȃ��B
            modelIndexList.push_back(modelVertexListOpt.size());
            modelVertexListOpt.push_back(vertex);
        }
        else {
            //�d�����Ă���
            auto index = std::distance(modelVertexListOpt.begin(), it);
            modelIndexList.push_back(index);
        }
    }
    printf("Opt: %lu -> %lu\n", modelVertexList.size(), modelVertexListOpt.size());
    return modelMesh;
}

//����t���[���ɂ�����
void FbxLoader::GetMeshMatrix(float frame, int meshId, glm::mat4 *out_matrix, int animNum) const {
    auto& modelMesh = this->mMeshList[meshId];
    auto it = this->mAnimationArray[animNum].nodeIdDictionaryAnimation.find(modelMesh.nodeName);

    if (it == this->mAnimationArray[animNum].nodeIdDictionaryAnimation.end()) {
        *out_matrix = glm::mat4(1.0);
        return;	//���������݂���̂͊�̂����B
    }
    assert(animNum < this->mAnimationArray.size());
    auto meshNodeId = it->second;
    auto meshNode = this->mAnimationArray[animNum].fbxSceneAnimation->GetNode(meshNodeId);

    FbxTime time;
    time.Set(FbxTime::GetOneFrameValue(FbxTime::eFrames60)*frame);

    auto& meshMatrix = meshNode->EvaluateGlobalTransform(time);
    auto meshMatrixPtr = (double*)meshMatrix;
    for (int i = 0; i < 16; i++) {
        (*out_matrix)[i / 4][i % 4] = (float)meshMatrixPtr[i];
    }
    *out_matrix = *out_matrix* modelMesh.invMeshBaseposeMatrix;
}

void FbxLoader::GetBoneMatrix(float frame, int meshId, glm::mat4 *out_matrixList, int matrixCount, int animNum) const {
    auto& modelMesh = this->mMeshList[meshId];
    if (modelMesh.boneNodeNameList.size() == 0)
    {
        out_matrixList[0] = glm::mat4(1.0);
        //printf("no bone\n");
        return;
    }
    assert(modelMesh.boneNodeNameList.size() <= matrixCount);
    assert(animNum < this->mAnimationArray.size());

    FbxTime time;
    time.Set(FbxTime::GetOneFrameValue(FbxTime::eFrames60) * frame);

    unsigned int size = modelMesh.boneNodeNameList.size();
    for (unsigned int i = 0; i < size; ++i)
    {
        auto& boneNodeName = modelMesh.boneNodeNameList[i];
        int boneNodeId = this->mAnimationArray[animNum].nodeIdDictionaryAnimation.at(boneNodeName);
        auto boneNode = this->mAnimationArray[animNum].fbxSceneAnimation->GetNode(boneNodeId);

        auto& boneMatrix = boneNode->EvaluateGlobalTransform(time);
        auto& out_matrix = out_matrixList[i];

        auto boneMatrixPtr = (double*)boneMatrix;
        for (int j = 0; j < 16; j++) {
            out_matrix[j / 4][j % 4] = (float)boneMatrixPtr[j];
        }
        out_matrix = out_matrix * modelMesh.invBoneBaseposeMatrixList[i];
    }

}
void FbxLoader::GetBoneMatrix(float frame, int meshId, glm::mat4 *out_matrixList, glm::mat4 *matrixList, int matrixCount, int animNum) const {
    auto& modelMesh = this->mMeshList[meshId];
    unsigned int size = modelMesh.boneNodeNameList.size();
    for (unsigned int i = 0; i < size; ++i)
    {
        out_matrixList[i] = matrixList[i];
    }

}

void FbxLoader::ParseMaterialList(FbxMesh *mesh)
{
    FbxNode *node = mesh->GetNode();
    int matCount = node->GetMaterialCount();
    FBXSDK_printf("material count: %d\n", matCount);
    for (int i = 0; i < matCount; i++) {
        FbxSurfaceMaterial *material = node->GetMaterial(i);
        ParseMaterial(material);
    }
}

void FbxLoader::ParseMaterial(FbxSurfaceMaterial* material) {
    ModelMaterial mtl;

    auto implementation = GetImplementation(material, FBXSDK_IMPLEMENTATION_CGFX);
    if (implementation) {
        //if (material->GetClassId().Is(FbxSurfaceLambert::ClassId)){
        auto rootTable = implementation->GetRootTable();
        auto entryCount = rootTable->GetEntryCount();
        for (int k = 0; k < entryCount; k++) {
            auto entry = rootTable->GetEntry(k);

            auto fbxProperty = material->FindPropertyHierarchical(entry.GetSource());
            if (!fbxProperty.IsValid()) {
                fbxProperty = material->RootProperty.FindHierarchical(entry.GetSource());
            }

            auto textureCount = fbxProperty.GetSrcObjectCount<FbxTexture>();
            if (textureCount > 0) {
                std::string src = entry.GetSource();

                for (int j = 0; j < fbxProperty.GetSrcObjectCount<FbxFileTexture>(); j++) {
                    auto tex = fbxProperty.GetSrcObject<FbxFileTexture>(j);
                    std::string texName = tex->GetFileName();
                    texName = texName.substr(texName.find_last_of('/') + 1);

                    if (src == "Maya|DiffuseTexture") {
                        mtl.diffuseTextureName = texName;
                    }
                    else if (src == "Maya|NormalTexture") {
                        mtl.normalTextureName = texName;
                    }
                    else if (src == "Maya|SpecularTexture") {
                        mtl.specularTextureName = texName;
                    }
                    else if (src == "Maya|FalloffTexture") {
                        mtl.falloffTextureName = texName;
                    }
                    else if (src == "Maya|ReflectionMapTexture") {
                        mtl.reflectionMapTextureName = texName;
                    }
                }
            }
        }
        mtl.materialName = material->GetName();
        printf("diffuseTexture: %s\n", mtl.diffuseTextureName.c_str());
        printf("normalTexture: %s\n", mtl.normalTextureName.c_str());
        printf("specularTexture: %s\n", mtl.specularTextureName.c_str());
        printf("falloffTexture: %s\n", mtl.falloffTextureName.c_str());
        printf("reflectionMapTexture: %s\n", mtl.reflectionMapTextureName.c_str());
        this->mMaterialList.push_back(mtl);
        this->mMaterialIdDictionary.insert({ mtl.materialName, mMaterialNum }); //�����o�^
        FBXSDK_printf("materialName:%d:%s\n", mMaterialNum, mtl.materialName.c_str());
        mMaterialNum++;
    }
    else {
        FbxProperty lProperty = material->FindProperty(FbxSurfaceMaterial::sDiffuse);
        int layerNum = lProperty.GetSrcObjectCount<FbxLayeredTexture>();
        if (layerNum == 0) {
            int fileTextureCount = lProperty.GetSrcObjectCount<FbxFileTexture>();
            FBXSDK_printf("Texture Count:%d\n", fileTextureCount);
            for (int j = 0; j < fileTextureCount; j++) {
                FbxFileTexture* pFileTexture = FbxCast<FbxFileTexture>(lProperty.GetSrcObject<FbxTexture>(j));
                std::string texName = (char*)pFileTexture->GetFileName();
                texName = texName.substr(texName.find_last_of('/') + 1);

                mtl.diffuseTextureName = texName;
                mtl.materialName = material->GetName();
                this->mMaterialList.push_back(mtl);
                this->mMaterialIdDictionary.insert({ mtl.materialName, mMaterialNum }); //�����o�^
                FBXSDK_printf("diffuseTexture: %s\n", mtl.diffuseTextureName.c_str());
                FBXSDK_printf("materialName:%d:%s\n", mMaterialNum, mtl.materialName.c_str());
                mMaterialNum++;
            }
        }
    }
}

}