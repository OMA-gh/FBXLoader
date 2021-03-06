// main.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"

#include <string>

#include <fbx.h>

int main()
{
    std::string inputfile = "Resources/oma_2.fbx";
    std::string anim_inputfile = "Resources/oma_2_anim.fbx";

    fbx::FbxLoader* fbx_model = new fbx::FbxLoader();
    int i = 0;
    if (fbx_model->Initialize(inputfile.c_str(), anim_inputfile.c_str())) {
        auto it = fbx_model->GetMeshList().begin();
        auto end = fbx_model->GetMeshList().end();
        for (; it != end; it++) {
            printf("name:%s\n", it->nodeName.c_str());
            printf("material name:%s\n", it->materialName.c_str());
            // set vertex list
            {
                auto v_it = it->vertexList.begin();
                auto v_end = it->vertexList.end();
                for (; v_it != v_end; v_it++) {
                    {
                        glm::vec3 push_pos(
                            v_it->position.x,
                            v_it->position.y,
                            v_it->position.z
                        );
                        //printf("pos[ %f, %f, %f ]\n", push_pos.x, push_pos.y, push_pos.z);
                    }
                    {
                        glm::vec3 push_normal(
                            v_it->normal.x,
                            v_it->normal.y,
                            v_it->normal.z
                        );
                        //printf("normal[ %f, %f, %f ]\n", push_normal.x, push_normal.y, push_normal.z);
                    }
                    {
                        glm::vec2 push_tex(
                            v_it->uv.x,
                            v_it->uv.y
                        );
                        //printf("uv[ %f, %f ]\n", push_tex.x, push_tex.y);
                    }
                    {
                        printf("bone:\n");
                        for (int i = 0; i < 4; i++) {
                            printf("- %d  bone_index[%d], bone weight[%f]\n", i, v_it->boneIndex[i], v_it->boneWeight[i]);
                        }
                    }
                }
            }
            // set index
            {
                auto i_it = it->indexList.begin();
                auto i_end = it->indexList.end();
                for (; i_it != i_end; i_it++) {
                    //printf("index[ %d ]\n", *i_it);
                }
            }
            {
                auto m_it = fbx_model->GetMaterialList().begin();
                auto m_end = fbx_model->GetMaterialList().end();
                for (; m_it != m_end; m_it++) {
                    if (it->materialName == m_it->materialName) {
                        printf("tex name: %s\n", m_it->diffuseTextureName.c_str());
                    }
                }
            }
        }
    }
    else {
        printf("[Load FAILED] %s", inputfile.c_str());
    }
    printf("[Load END]");
    while (true);
    return 0;
}

