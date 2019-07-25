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
    this->Finalize();
}

bool FbxLoader::Initialize(const char* filepath, const char* animetion_path) {
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

    //ポリゴンの三角形化を行う
    FbxGeometryConverter geometry_converter(this->mManagerPtr);
    geometry_converter.Triangulate(this->mScenePtr, true);

    //辞書に登録し、ノード名からノードのIDを取得できるようにする
    auto node_count = this->mScenePtr->GetNodeCount();
    printf("NodeCount: %d\n", node_count);
    for (int i = 0; i < node_count; i++) {
        auto fbxNode = this->mScenePtr->GetNode(i);
        this->mNodeIdDictionary.insert({ fbxNode->GetName(),i });
    }

    //マテリアルの解析
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

    this->LoadAnimation(animetion_path);
    
    return true;
}
//------------------------------------------------------------------------------------------
//メッシュに置けるインデックスのリストを返す
void GetIndexList(std::vector<int>* index_list, const FbxMesh& mesh) {
    int polygon_count = mesh.GetPolygonCount();
    FBXSDK_printf("-----------------------------------------------\n");
    FBXSDK_printf("loading index start\n");
    FBXSDK_printf("polygon count:[%d]\n", polygon_count);

    index_list->reserve(polygon_count * 3);
    for (int i = 0; i < polygon_count; i++) {
        index_list->push_back(mesh.GetPolygonVertex(i, 0));
        index_list->push_back(mesh.GetPolygonVertex(i, 1));
        index_list->push_back(mesh.GetPolygonVertex(i, 2));
    }
    FBXSDK_printf("-----------------------------------------------\n");
}
//------------------------------------------------------------------------------------------
//頂点の位置座標のリストを返す
void GetPositionList(std::vector<glm::vec3>* position_list, const FbxMesh& mesh, const std::vector<int>& indexList) {
    FBXSDK_printf("-----------------------------------------------\n");
    FBXSDK_printf("loading position start\n");
    position_list->reserve(indexList.size());

    for (int index : indexList) {
        auto control_point = mesh.GetControlPointAt(index);
        position_list->push_back(glm::vec3(control_point[0], control_point[1], control_point[2]));
    }
    FBXSDK_printf("-----------------------------------------------\n");
}
//------------------------------------------------------------------------------------------
//これはメッシュの法線のリストを返す
void GetNormalList(std::vector<glm::vec3>* normal_list, const FbxMesh& mesh, const std::vector<int> &index_list) {
    FBXSDK_printf("-----------------------------------------------\n");
    FBXSDK_printf("loading normal start\n");
    int element_count = mesh.GetElementNormalCount();
    auto element = mesh.GetElementNormal();
    auto mapping_mode = element->GetMappingMode();
    auto referenceMode = element->GetReferenceMode();
    const auto& indexArray = element->GetIndexArray();
    const auto& directArray = element->GetDirectArray();

    normal_list->reserve(index_list.size());

    //マッピングモードでfbxファイル内の法線のデータが違う
    if (mapping_mode == FbxGeometryElement::eByControlPoint) {
        for (auto index : index_list) {
            auto normalIndex = (referenceMode == FbxGeometryElement::eDirect) ?
                index : indexArray.GetAt(index);
            //リファレンスモードがeDirectならば、法線はそのまま入っている。
            //ソレ以外、大体eIndextoVertexかなんかだったらインデックス配列の番号を参照した
            //法線のデータがあるので其の番号を順番に取り出す
            auto normal = directArray.GetAt(normalIndex);
            normal_list->push_back(glm::vec3(normal[0], normal[1], normal[2]));
        }
    }
    else if (mapping_mode == FbxGeometryElement::eByPolygonVertex) {
        auto indexByPolygonVertex = 0;
        auto polygonCount = mesh.GetPolygonCount();
        for (int i = 0; i < polygonCount; i++) {
            auto polygonSize = mesh.GetPolygonSize(i); //これは三角化しているので全部3だと思う
            for (int j = 0; j < polygonSize; j++) {
                auto normalIndex = (referenceMode == FbxGeometryElement::eDirect)
                    ? indexByPolygonVertex
                    : indexArray.GetAt(indexByPolygonVertex);
                auto normal = directArray.GetAt(normalIndex);
                normal_list->push_back(glm::vec3(normal[0], normal[1], normal[2]));
                ++indexByPolygonVertex;
            }
        }
    }
    else {
        printf("unknown mapping mode\n");
        assert(false);
    }
    FBXSDK_printf("-----------------------------------------------\n");
}
//------------------------------------------------------------------------------------------
//メッシュ毎のテクスチャ座標をリストにして返す
void GetUVList(std::vector<glm::vec2>* uv_list,const FbxMesh& mesh, const std::vector<int>& index_list, int uv_no) {
    FBXSDK_printf("-----------------------------------------------\n");
    FBXSDK_printf("loading uv start\n");
    int element_cout = mesh.GetElementUVCount();
    if (uv_no + 1 > element_cout) {
        return;
    }
    auto element = mesh.GetElementUV(uv_no);
    auto mapping_mode = element->GetMappingMode();
    auto reference_mode = element->GetReferenceMode();
    const auto& index_array = element->GetIndexArray();
    const auto& direct_array = element->GetDirectArray();

    uv_list->reserve(index_list.size());
    if (mapping_mode == FbxGeometryElement::eByControlPoint) {
        //法線とだいたい同じ
        for (auto index : index_list) {
            auto uvIndex = (reference_mode == FbxGeometryElement::eDirect)
                ? index
                : index_array.GetAt(index);
            auto uv = direct_array.GetAt(uvIndex);
            uv_list->push_back(glm::vec2(uv[0], uv[1]));
        }
    }
    else if (mapping_mode == FbxGeometryElement::eByPolygonVertex) {
        int index_by_polygon_vetex = 0;
        int polygon_count = mesh.GetPolygonCount();
        for (int i = 0; i < polygon_count; i++) {
            int polygon_size = mesh.GetPolygonSize(i);
            for (int j = 0; j < polygon_size; j++) {
                int uv_index = (reference_mode == FbxGeometryElement::eDirect)
                    ? index_by_polygon_vetex
                    : index_array.GetAt(index_by_polygon_vetex);
                auto uv = direct_array.GetAt(uv_index);
                uv_list->push_back(glm::vec2(uv[0], uv[1]));
                ++index_by_polygon_vetex;
            }
        }
    }
    else {
        printf("unknown mapping mode\n");
        assert(false);
    }
    FBXSDK_printf("-----------------------------------------------\n");
}
//------------------------------------------------------------------------------------------
//ウェイトをメッシュごとに手に入れて返す
void GetWeight(std::vector<ModelBoneWeight>* bone_weight_list, std::vector<std::string>* bone_node_name_list, std::vector<glm::mat4>* inv_basepose_matrix_list, const FbxMesh& mesh, const std::vector<int>& index_list) {
    FBXSDK_printf("-----------------------------------------------\n");
    FBXSDK_printf("loading bone weight start\n");
    int skin_count = mesh.GetDeformerCount(FbxDeformer::eSkin);
    FBXSDK_printf("skin count:[%d]\n", skin_count);
    if (skin_count == 0) {
        FBXSDK_printf("-----------------------------------------------\n");
        return;
    }
    assert(skin_count <= 1);

    int control_points_count = mesh.GetControlPointsCount();
    using TmpWeight = std::pair<int, float>;
    std::vector<std::vector<TmpWeight>> tmp_bone_weight_list(control_points_count);
    FBXSDK_printf("mesh control point count:%d\n", control_points_count);

    auto skin = static_cast<FbxSkin*>(mesh.GetDeformer(0, FbxDeformer::eSkin));

    int cluster_count = skin->GetClusterCount();
    FBXSDK_printf("cluster count:[%d]\n", cluster_count);
    for (int i = 0; i < cluster_count; i++) {
        auto cluster = skin->GetCluster(i);

        assert(cluster->GetLinkMode() == FbxCluster::eNormalize);

        bone_node_name_list->push_back(cluster->GetLink()->GetName());
        FBXSDK_printf("cluster name:[%s]\n",cluster->GetLink()->GetName());

        int index_count = cluster->GetControlPointIndicesCount();
        auto indices = cluster->GetControlPointIndices();
        auto weights = cluster->GetControlPointWeights();

        for (int k = 0; k < index_count; k++) {
            int control_point_index = indices[k];
            tmp_bone_weight_list[control_point_index].push_back({ i, weights[k] });
        }

        glm::mat4 inverse_base_pose_matrix; //ベースポーズの逆行列、どっかで使うため

        auto basepose_matrix = cluster->GetLink()->EvaluateGlobalTransform().Inverse();
        for (int k = 0; k < 16; k++) {
            inverse_base_pose_matrix[k / 4][k % 4] = (float)(basepose_matrix[k / 4][k % 4]);
        }
        inv_basepose_matrix_list->push_back(inverse_base_pose_matrix);
    }

    std::vector<ModelBoneWeight> bone_weight_list_control_points;
    FBXSDK_printf("bone weight count:[%d]\n", (int)tmp_bone_weight_list.size());
    for (auto& tmp_bone_weight : tmp_bone_weight_list) {
        //ウェイトの大きさでソート
        std::sort(tmp_bone_weight.begin(), tmp_bone_weight.end(), [](const TmpWeight& weightA, const TmpWeight& weightB) { return weightA.second > weightB.second; });
        //Unitychan.fbxとかにはウェイトが6つまで用意されている為、GLSLで使いやすいように4つに減らすということをしている
        while (tmp_bone_weight.size() > 4) {
            tmp_bone_weight.pop_back();
        }

        while (tmp_bone_weight.size() < 4) {
            tmp_bone_weight.push_back({ 0, 0.0f }); //ダミーを入れとく
        }
        ModelBoneWeight weight;
        float total = 0.0f;
        for (int i = 0; i < 4; i++) {
            weight.boneIndex[i] = tmp_bone_weight[i].first;
            weight.boneWeight[i] = tmp_bone_weight[i].second;
            total += tmp_bone_weight[i].second;
        }
        //正規化
        for (int i = 0; i < 4; i++) {
            weight.boneWeight[i] /= total;
        }
        FBXSDK_printf("push weight %d ([0]index:%d,weight:%f)([1]index:%d,weight:%f)([2]index:%d,weight:%f)([3]index:%d,weight:%f)\n",
            (int)bone_weight_list_control_points.size(),
            weight.boneIndex[0], weight.boneWeight[0],
            weight.boneIndex[1], weight.boneWeight[1],
            weight.boneIndex[2], weight.boneWeight[2],
            weight.boneIndex[3], weight.boneWeight[3]
        );
        bone_weight_list_control_points.push_back(weight);
    }

    for (int index : index_list) {
        bone_weight_list->push_back(bone_weight_list_control_points[index]);
    }
    FBXSDK_printf("-----------------------------------------------\n");
}
//------------------------------------------------------------------------------------------
//アニメーションを得る。
//**animationStartFrame ->始まりのフレーム
//**animationEndFrame ->終わりのフレーム
//**nodeIdDictionaryAnimation ->アニメーションのノードを辞書型にリストに入れておく
bool FbxLoader::LoadAnimation(const char* filepath)
{
    auto importer = FbxImporter::Create(this->mManagerPtr, "");

    if (!importer->Initialize(filepath, -1, this->mManagerPtr->GetIOSettings())) {
        FBXSDK_printf("Animation Load Error!:%s\n", filepath);
        return false;
    }
    FbxAnimation fbx_animation;

    FBXSDK_printf("-----------------------------------------------\n");
    FBXSDK_printf("loading animation start:%s\n", filepath);
    fbx_animation.fbxSceneAnimation = FbxScene::Create(this->mManagerPtr, "animationScene");
    importer->Import(fbx_animation.fbxSceneAnimation);

    int animStackCount = importer->GetAnimStackCount();
    FBXSDK_printf("[ImporterName: %s] [StackCount: %d]\n", importer->GetName(), animStackCount);
    assert(animStackCount > 0);
    for (int i = 0; i < animStackCount; i++) {
        auto take_info = importer->GetTakeInfo(i);

        FBXSDK_printf("[AnimationName: %s] [index: %d]\n", take_info->mName.Buffer(), i);
        auto import_offset = take_info->mImportOffset;
        auto start_time = take_info->mLocalTimeSpan.GetStart();
        auto stop_time = take_info->mLocalTimeSpan.GetStop();

        fbx_animation.animationStartFrame = (import_offset.Get() + start_time.Get()) / (float)FbxTime::GetOneFrameValue(FbxTime::eFrames60);
        fbx_animation.animationEndFrame = (import_offset.Get() + stop_time.Get()) / (float)FbxTime::GetOneFrameValue(FbxTime::eFrames60);

        // ノード名からノードIDを取得できるように辞書に登録
        int node_count = fbx_animation.fbxSceneAnimation->GetNodeCount();
        FBXSDK_printf("animationNodeCount: %d\n", node_count);
        for (int i = 0; i < node_count; ++i)
        {
            auto fbx_node = fbx_animation.fbxSceneAnimation->GetNode(i);
            fbx_animation.nodeIdDictionaryAnimation.insert({ fbx_node->GetName(), i });
            FBXSDK_printf("- add node [bone name:[%s], index:[%d]]\n", fbx_node->GetName(), i);
        }
        FBXSDK_printf("strtframe %f\n", fbx_animation.GetAnimationStartFrame());
        FBXSDK_printf("endframe %f\n", fbx_animation.GetAnimationEndFrame());
        FBXSDK_printf("-----------------------------------------------\n");
        this->mAnimationArray.push_back(fbx_animation);
    }
    importer->Destroy();
    return true;
}
//------------------------------------------------------------------------------------------
void FbxLoader::Finalize() {
    this->mMeshList.clear();
    this->mMaterialList.clear();
    this->mMaterialIdDictionary.clear();
    this->mNodeIdDictionary.clear();
}
//------------------------------------------------------------------------------------------
//ノードを巡る
void FbxLoader::ParseNode(FbxNode *node) {
    FBXSDK_printf("-----------------------------------------------\n");
    FBXSDK_printf("parse node\n");
    FBXSDK_printf("node name:%s\n", node->GetName());
    int child_num = node->GetChildCount();
    FBXSDK_printf("node count:%d\n", child_num);
    FbxNode *child_node = 0;
    for (int i = 0; i < child_num; i++) {
        child_node = node->GetChild(i);
        FBXSDK_printf("child[%d] name:%s\n", i, child_node->GetName());
        FbxMesh* mesh = child_node->GetMesh();
        if (mesh != NULL) {
            FBXSDK_printf("-----------------------------------------------\n");
            FBXSDK_printf("parse mesh\n");
            this->mMeshList.push_back(this->ParseMesh(mesh));
            ParseMaterialList(mesh);
            FBXSDK_printf("--parse mesh-----------------------------------\n");
        }
        ParseNode(child_node);
    }
    FBXSDK_printf("-parse node end--------------------------------\n");
}
//------------------------------------------------------------------------------------------
//得られたメッシュを走査して頂点・インデックス・法線・ウェイト・ボーンインデックスを得てリストにプッシュする
ModelMesh FbxLoader::ParseMesh(FbxMesh *mesh) {
    auto node = mesh->GetNode();

    ModelMesh model_mesh;
    model_mesh.nodeName = node->GetName();
    if (fbxsdk::FbxSurfaceMaterial* mt = node->GetMaterial(0)) {
        model_mesh.materialName = mt->GetName();
    }
    else {
        model_mesh.materialName = "";
    }
    printf(">> mesh: %s\n", model_mesh.nodeName.c_str());
    //ベースポーズの逆行列？
    auto basepose_matrix = node->EvaluateGlobalTransform().Inverse();
    auto basepose_matrix_ptr = (double*)basepose_matrix;

    for (int i = 0; i < 16; i++) {
        model_mesh.invMeshBaseposeMatrix[i / 4][i % 4] = (float)basepose_matrix_ptr[i];
    }

    //インデックス取得フェイズ
    std::vector<int> index_list;
    GetIndexList(&index_list, *mesh);
    //頂点を取得
    std::vector<glm::vec3> position_list;
    GetPositionList(&position_list, *mesh, index_list);
    FBXSDK_printf("*[position num : %d]*\n", (int)position_list.size());
    std::vector<glm::vec3> normal_list;
    GetNormalList(&normal_list, *mesh, index_list);
    FBXSDK_printf("*[normal num : %d]*\n", (int)normal_list.size());
    std::vector<glm::vec2> uv_list;
    GetUVList(&uv_list, *mesh, index_list, 0);
    FBXSDK_printf("*[uv num : %d]*\n", (int)uv_list.size());

    // ボーン取得
    std::vector<ModelBoneWeight> bone_weight_list;
    GetWeight(&bone_weight_list, &model_mesh.boneNodeNameList, &model_mesh.invBoneBaseposeMatrixList, *mesh, index_list);

    std::vector<ModelVertex> model_vertex_list;
    model_vertex_list.reserve(index_list.size());

    for (unsigned int i = 0; i < index_list.size(); i++) {
        ModelVertex vertex;
        vertex.position = position_list[i];
        vertex.normal = normal_list[i];
        vertex.uv = (uv_list.size() == 0) ? glm::vec2(0.0f, 0.0f) : uv_list[i];
        if (bone_weight_list.size() > 0) {
            for (int j = 0; j < 4; ++j) {
                vertex.boneIndex[j] = bone_weight_list[i].boneIndex[j];
            }
            vertex.boneWeight = bone_weight_list[i].boneWeight;
        }
        else {
            for (int j = 0; j < 4; ++j) {
                vertex.boneIndex[j] = 0;
            }
            vertex.boneWeight = glm::vec4(1, 0, 0, 0);
        }
        model_vertex_list.push_back(vertex);
    }

    //glDrawArrays()による描画が可能になる。
    //インデックスのターン
    //重複頂点を除く
    auto& model_vertex_list_opt = model_mesh.vertexList;
    model_vertex_list_opt.reserve(model_vertex_list.size());

    auto& model_index_list = model_mesh.indexList;
    model_index_list.reserve(index_list.size());

    for (auto& vertex : model_vertex_list) {
        //重複しているか
        auto it = std::find(model_vertex_list_opt.begin(), model_vertex_list_opt.end(), vertex);
        if (it == model_vertex_list_opt.end()) {
            //itがリストの最後を指しているので、重複していない。
            model_index_list.push_back((unsigned short)model_vertex_list_opt.size());
            model_vertex_list_opt.push_back(vertex);
        }
        else {
            //重複している
            auto index = std::distance(model_vertex_list_opt.begin(), it);
            model_index_list.push_back((unsigned short)index);
        }
    }
    FBXSDK_printf("Opt: %lu -> %lu\n", (unsigned short)model_vertex_list.size(), (unsigned short)model_vertex_list_opt.size());
    return model_mesh;
}
//------------------------------------------------------------------------------------------
//あるフレームにおける
void FbxLoader::GetMeshMatrix(glm::mat4 *out_matrix, float frame, int mesh_id, int anim_num) const {
    auto& model_mesh = this->mMeshList[mesh_id];
    auto it = this->mAnimationArray[anim_num].nodeIdDictionaryAnimation.find(model_mesh.nodeName);

    if (it == this->mAnimationArray[anim_num].nodeIdDictionaryAnimation.end()) {
        *out_matrix = glm::mat4(1.0);
        return;
    }
    assert(anim_num < this->mAnimationArray.size());
    auto mesh_node_id = it->second;
    auto mesh_node = this->mAnimationArray[anim_num].fbxSceneAnimation->GetNode(mesh_node_id);

    FbxTime time;
    time.Set(FbxTime::GetOneFrameValue(FbxTime::eFrames60)*(fbxsdk::FbxLongLong)frame);

    auto& mesh_matrix = mesh_node->EvaluateGlobalTransform(time);
    auto mesh_matrix_ptr = (double*)mesh_matrix;
    for (int i = 0; i < 16; i++) {
        (*out_matrix)[i / 4][i % 4] = (float)mesh_matrix_ptr[i];
    }
    *out_matrix = *out_matrix* model_mesh.invMeshBaseposeMatrix;
}
//------------------------------------------------------------------------------------------
void FbxLoader::GetBoneMatrix(glm::mat4 *out_matrix_list, float frame, int meshId, int matrix_count, int anim_num) const {
    auto& model_mesh = this->mMeshList[meshId];
    if (model_mesh.boneNodeNameList.size() == 0)
    {
        out_matrix_list[0] = glm::mat4(1.0);
        FBXSDK_printf("no bone\n");
        return;
    }
    assert(model_mesh.boneNodeNameList.size() <= matrix_count);
    assert(anim_num < this->mAnimationArray.size());

    FbxTime time;
    time.Set(FbxTime::GetOneFrameValue(FbxTime::eFrames60) * (fbxsdk::FbxLongLong)frame);

    unsigned int size = (unsigned int)model_mesh.boneNodeNameList.size();
    for (unsigned int i = 0; i < size; ++i)
    {
        auto& bone_node_name = model_mesh.boneNodeNameList[i];
        int bone_node_id = this->mAnimationArray[anim_num].nodeIdDictionaryAnimation.at(bone_node_name);
        auto bone_node = this->mAnimationArray[anim_num].fbxSceneAnimation->GetNode(bone_node_id);

        auto& bone_matrix = bone_node->EvaluateGlobalTransform(time);
        auto& out_matrix = out_matrix_list[i];

        auto bone_matrix_ptr = (double*)bone_matrix;
        for (int j = 0; j < 16; j++) {
            out_matrix[j / 4][j % 4] = (float)bone_matrix_ptr[j];
        }
        out_matrix = out_matrix * model_mesh.invBoneBaseposeMatrixList[i];
    }

}
//------------------------------------------------------------------------------------------
void FbxLoader::GetBoneMatrix(glm::mat4 *out_matrix_list, float frame, int mesh_id, glm::mat4* matrix_list, int matrix_count, int anim_num) const {
    auto& model_mesh = this->mMeshList[mesh_id];
    unsigned int size = (unsigned int)model_mesh.boneNodeNameList.size();
    for (unsigned int i = 0; i < size; ++i)
    {
        out_matrix_list[i] = matrix_list[i];
    }
}
//------------------------------------------------------------------------------------------
void FbxLoader::ParseMaterialList(FbxMesh *mesh)
{
    FbxNode *node = mesh->GetNode();
    int material_count = node->GetMaterialCount();
    FBXSDK_printf("material count: %d\n", material_count);
    for (int i = 0; i < material_count; i++) {
        FbxSurfaceMaterial *material = node->GetMaterial(i);
        ParseMaterial(material);
    }
}
//------------------------------------------------------------------------------------------
void FbxLoader::ParseMaterial(FbxSurfaceMaterial* material) {
    ModelMaterial mtl;

    auto implementation = GetImplementation(material, FBXSDK_IMPLEMENTATION_CGFX);
    if (implementation) {
        //if (material->GetClassId().Is(FbxSurfaceLambert::ClassId)){
        auto root_table = implementation->GetRootTable();
        auto entry_count = root_table->GetEntryCount();
        for (int k = 0; k < entry_count; k++) {
            auto entry = root_table->GetEntry(k);

            auto fbx_property = material->FindPropertyHierarchical(entry.GetSource());
            if (!fbx_property.IsValid()) {
                fbx_property = material->RootProperty.FindHierarchical(entry.GetSource());
            }

            int texture_count = fbx_property.GetSrcObjectCount<FbxTexture>();
            if (texture_count > 0) {
                std::string src = entry.GetSource();

                for (int j = 0; j < fbx_property.GetSrcObjectCount<FbxFileTexture>(); j++) {
                    auto tex = fbx_property.GetSrcObject<FbxFileTexture>(j);
                    std::string tex_name = tex->GetFileName();
                    tex_name = tex_name.substr(tex_name.find_last_of('/') + 1);

                    if (src == "Maya|DiffuseTexture") {
                        mtl.diffuseTextureName = tex_name;
                    }
                    else if (src == "Maya|NormalTexture") {
                        mtl.normalTextureName = tex_name;
                    }
                    else if (src == "Maya|SpecularTexture") {
                        mtl.specularTextureName = tex_name;
                    }
                    else if (src == "Maya|FalloffTexture") {
                        mtl.falloffTextureName = tex_name;
                    }
                    else if (src == "Maya|ReflectionMapTexture") {
                        mtl.reflectionMapTextureName = tex_name;
                    }
                }
            }
        }
        mtl.materialName = material->GetName();
        printf("diffuseTexture      : %s\n", mtl.diffuseTextureName.c_str());
        printf("normalTexture       : %s\n", mtl.normalTextureName.c_str());
        printf("specularTexture     : %s\n", mtl.specularTextureName.c_str());
        printf("falloffTexture      : %s\n", mtl.falloffTextureName.c_str());
        printf("reflectionMapTexture: %s\n", mtl.reflectionMapTextureName.c_str());
        this->mMaterialList.push_back(mtl);
        this->mMaterialIdDictionary.insert({ mtl.materialName, mMaterialNum }); //辞書登録
        FBXSDK_printf("materialName:%d:%s\n", mMaterialNum, mtl.materialName.c_str());
        mMaterialNum++;
    }
    else {
        FbxProperty property = material->FindProperty(FbxSurfaceMaterial::sDiffuse);
        int layer_num = property.GetSrcObjectCount<FbxLayeredTexture>();
        if (layer_num == 0) {
            int file_texture_count = property.GetSrcObjectCount<FbxFileTexture>();
            FBXSDK_printf("Texture Count:%d\n", file_texture_count);
            for (int j = 0; j < file_texture_count; j++) {
                FbxFileTexture* file_texture = FbxCast<FbxFileTexture>(property.GetSrcObject<FbxTexture>(j));
                std::string tex_name = (char*)file_texture->GetFileName();
                tex_name = tex_name.substr(tex_name.find_last_of('/') + 1);

                mtl.diffuseTextureName = tex_name;
                mtl.materialName = material->GetName();
                this->mMaterialList.push_back(mtl);
                this->mMaterialIdDictionary.insert({ mtl.materialName, mMaterialNum }); //辞書登録
                FBXSDK_printf("diffuseTexture: %s\n", mtl.diffuseTextureName.c_str());
                FBXSDK_printf("materialName:%d:%s\n", mMaterialNum, mtl.materialName.c_str());
                mMaterialNum++;
            }
        }
    }
}
//------------------------------------------------------------------------------------------
} // fbx