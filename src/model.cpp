#include "model.hpp"
#include <cgltf.h>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>

// ── Helpers ──────────────────────────────────────────────────────────────────

static std::string dirOf(const std::string& path) {
    auto pos = path.rfind('/');
    return pos == std::string::npos ? "." : path.substr(0, pos);
}

// Read a single float element from an accessor by index (handles stride + offset).
static float readFloat(const cgltf_accessor* acc, cgltf_size idx, int component) {
    const cgltf_buffer_view* bv = acc->buffer_view;
    const char* base = static_cast<const char*>(bv->buffer->data)
                       + bv->offset + acc->offset;
    cgltf_size stride = bv->stride ? bv->stride : cgltf_calc_size(acc->type, acc->component_type);
    const float* v = reinterpret_cast<const float*>(base + idx * stride);
    return v[component];
}

// ── Node traversal ────────────────────────────────────────────────────────────

static void walkNodes(const cgltf_node* const* nodes, cgltf_size count,
                      const glm::mat4& parentTransform,
                      const std::string& dir,
                      std::vector<SubMesh>& submeshes,
                      glm::mat4& outTransform)
{
    for (cgltf_size ni = 0; ni < count; ++ni) {
        const cgltf_node* node = nodes[ni];

        // Accumulate local transform.
        glm::mat4 local(1.0f);
        if (node->has_matrix) {
            // glTF matrix is column-major float[16], same layout as glm::mat4.
            local = glm::make_mat4(node->matrix);
        } else {
            float tmp[16];
            cgltf_node_transform_local(node, tmp);
            local = glm::make_mat4(tmp);
        }
        glm::mat4 world = parentTransform * local;

        if (node->mesh) {
            outTransform = world;  // store transform at the mesh node

            for (cgltf_size pi = 0; pi < node->mesh->primitives_count; ++pi) {
                const cgltf_primitive& prim = node->mesh->primitives[pi];
                if (prim.type != cgltf_primitive_type_triangles) continue;

                // ── Find attribute accessors ──────────────────────────
                const cgltf_accessor* posAcc  = nullptr;
                const cgltf_accessor* normAcc = nullptr;
                const cgltf_accessor* uvAcc   = nullptr;

                for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai) {
                    const cgltf_attribute& a = prim.attributes[ai];
                    if (a.type == cgltf_attribute_type_position)  posAcc  = a.data;
                    if (a.type == cgltf_attribute_type_normal)    normAcc = a.data;
                    if (a.type == cgltf_attribute_type_texcoord && a.index == 0) uvAcc = a.data;
                }
                if (!posAcc) continue;

                const cgltf_size vCount = posAcc->count;

                // ── Build interleaved Vertex array ────────────────────
                std::vector<Vertex> verts(vCount);
                for (cgltf_size vi = 0; vi < vCount; ++vi) {
                    verts[vi].x  = readFloat(posAcc,  vi, 0);
                    verts[vi].y  = readFloat(posAcc,  vi, 1);
                    verts[vi].z  = readFloat(posAcc,  vi, 2);

                    if (normAcc) {
                        verts[vi].nx = readFloat(normAcc, vi, 0);
                        verts[vi].ny = readFloat(normAcc, vi, 1);
                        verts[vi].nz = readFloat(normAcc, vi, 2);
                    }
                    if (uvAcc) {
                        verts[vi].u =       readFloat(uvAcc, vi, 0);
                        verts[vi].v = 1.0f - readFloat(uvAcc, vi, 1);  // glTF v=0 is top; GL v=0 is bottom
                    }
                }

                // ── Build index array ─────────────────────────────────
                const cgltf_accessor* idxAcc = prim.indices;
                std::vector<unsigned int> indices;
                if (idxAcc) {
                    indices.resize(idxAcc->count);
                    for (cgltf_size ii = 0; ii < idxAcc->count; ++ii)
                        indices[ii] = static_cast<unsigned int>(cgltf_accessor_read_index(idxAcc, ii));
                }

                // ── Resolve texture paths and load albedo ─────────────
                std::string albedoPath, normalPath, ormPath;
                if (prim.material) {
                    const cgltf_material* mat = prim.material;
                    auto resolve = [&](const cgltf_texture* tex) -> std::string {
                        if (tex && tex->image && tex->image->uri)
                            return dir + "/" + tex->image->uri;
                        return {};
                    };
                    albedoPath = resolve(mat->pbr_metallic_roughness.base_color_texture.texture);
                    ormPath    = resolve(mat->pbr_metallic_roughness.metallic_roughness_texture.texture);
                    normalPath = resolve(mat->normal_texture.texture);
                }

                // Fall back to white texture if no albedo found.
                Texture albedo = albedoPath.empty()
                    ? Texture::white()
                    : Texture(albedoPath);

                SubMesh sm{
                    Mesh(verts, indices),
                    std::move(albedo),
                    normalPath,
                    ormPath
                };
                submeshes.push_back(std::move(sm));
            }
        }

        // Recurse into children.
        if (node->children_count > 0)
            walkNodes(const_cast<const cgltf_node* const*>(node->children),
                      node->children_count, world, dir, submeshes, outTransform);
    }
}

// ── Model::loadGLTF ───────────────────────────────────────────────────────────

Model Model::loadGLTF(const std::string& path) {
    cgltf_options opts{};
    cgltf_data*   data = nullptr;

    if (cgltf_parse_file(&opts, path.c_str(), &data) != cgltf_result_success)
        throw std::runtime_error("cgltf: failed to parse " + path);

    if (cgltf_load_buffers(&opts, data, path.c_str()) != cgltf_result_success) {
        cgltf_free(data);
        throw std::runtime_error("cgltf: failed to load buffers for " + path);
    }

    if (cgltf_validate(data) != cgltf_result_success) {
        cgltf_free(data);
        throw std::runtime_error("cgltf: validation failed for " + path);
    }

    // Extract filename stem for model name.
    const std::string dir = dirOf(path);
    Model model;
    {
        auto slash = path.rfind('/');
        auto dot   = path.rfind('.');
        size_t start = (slash == std::string::npos) ? 0 : slash + 1;
        size_t len   = (dot != std::string::npos && dot > start) ? dot - start : std::string::npos;
        model.m_name = path.substr(start, len);
    }

    if (data->scene) {
        walkNodes(const_cast<const cgltf_node* const*>(data->scene->nodes),
                  data->scene->nodes_count,
                  glm::mat4(1.0f),
                  dir,
                  model.m_submeshes,
                  model.m_transform);
    }

    cgltf_free(data);

    if (model.m_submeshes.empty())
        throw std::runtime_error("cgltf: no renderable primitives found in " + path);

    // Bounding sphere in model space (transform applied as model matrix at draw time).
    model.m_boundingRadius = model.m_submeshes[0].mesh.boundingRadius();
    model.m_centre         = glm::vec3(0.0f);

    return model;
}

// ── Model methods ─────────────────────────────────────────────────────────────

void Model::draw(Shader& shader, const glm::mat4& modelMatrix) const {
    for (const SubMesh& sm : m_submeshes) {
        sm.albedo.bind(0);
        shader.set("uModel", modelMatrix);
        sm.mesh.draw();
    }
}

int Model::triangleCount() const {
    int total = 0;
    for (const SubMesh& sm : m_submeshes) total += sm.mesh.triangleCount();
    return total;
}

int Model::vertexCount() const {
    int total = 0;
    for (const SubMesh& sm : m_submeshes) total += sm.mesh.indexCount();
    return total;
}
