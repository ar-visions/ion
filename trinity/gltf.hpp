#pragma once
#include <mx/mx.hpp>

namespace ion {

namespace gltf {
    enums(ComponentType, NONE,
        NONE = 0,
        BYTE  = 5120, UNSIGNED_BYTE  = 5121,
        SHORT = 5122, UNSIGNED_SHORT = 5123,
        UNSIGNED_INT = 5125, FLOAT = 5126
    );

    enums(CompoundType, NONE,
        NONE   = 0,
        SCALAR = 1, VEC2 = 2, VEC3 = 3, VEC4 = 4,
        MAT2   = 5, MAT3 = 6, MAT4 = 7
    );

    enums(TargetType, NONE,
        NONE = 0,
        ARRAY_BUFFER = 34962, ELEMENT_BUFFER = 34963
    );

    enums(Mode, TRIANGLES,
        NONE = 0, LINES = 1, LINE_LOOP = 2,
        TRIANGLES = 4, TRIANGLE_STRIP = 5,
        TRIANGLE_FAN = 6, QUADS = 7
        /// Added magic QUADS since Khronos has abandoned its users including Facebook and Adobe
        /// They requested this feature when they were Facebook.
    );

    enums(Interpolation, LINEAR,
        LINEAR = 0, STEP = 1, CUBICSPLINE = 2
    );

    /// animation Sampler
    struct Sampler:mx {
        struct M {
            size_t        input;
            size_t        output;
            Interpolation interpolation;
            properties meta() {
                return {
                    prop { "input",          input },
                    prop { "output",         output },
                    prop { "interpolation",  interpolation },
                };
            }
        };
        mx_basic(Sampler);
    };

    struct ChannelTarget:mx {
        struct M {
            size_t          node;
            str             path; /// field name of node
            properties meta() {
                return {
                    prop { "node",           node },
                    prop { "path",           path }
                };
            }
        };
        mx_basic(ChannelTarget);
    };

    struct Channel:mx {
        struct M {
            size_t          sampler; /// sampler id
            ChannelTarget   target;
            properties meta() {
                return {
                    prop { "sampler",        sampler },
                    prop { "target",         target }
                };
            }
        };
        mx_basic(Channel);
    };

    struct Animation:mx {
        struct M {
            str             name;
            Array<Sampler>  samplers;
            Array<Channel>  channels;
            properties meta() {
                return {
                    prop { "name",           name },
                    prop { "samplers",       samplers },
                    prop { "channels",       channels }
                };
            }
        };
        mx_basic(Animation);
    };
    
    struct SparseInfo:mx {
        struct M {
            size_t          bufferView;
            ComponentType   componentType;
            /// componentType:
            /// sub-accessor type for bufferView, only for index types
            /// for value types, we are using the componentType of the accessor
            /// we assert that its undefined or set to the same
            properties meta() {
                return {
                    prop { "bufferView",     bufferView    },
                    prop { "componentType",  componentType }
                };
            }
        };
        mx_basic(SparseInfo);
    };

    struct Sparse:mx {
        struct M {
            size_t        count;
            SparseInfo    indices;
            SparseInfo    values;
            ///
            properties meta() {
                return {
                    prop { "count",          count       },
                    prop { "indices",        indices     },
                    prop { "values",         values      }
                };
            }
        };
        mx_basic(Sparse);
    };

    struct Accessor:mx {
        struct M {
            size_t        bufferView;
            ComponentType componentType;
            CompoundType  type;
            size_t        count;
            Array<float>  min;
            Array<float>  max;
            Sparse        sparse;
            ///
            properties meta() {
                return {
                    prop { "bufferView",     bufferView    },
                    prop { "componentType",  componentType },
                    prop { "count",          count         },
                    prop { "type",           type          },
                    prop { "min",            min           },
                    prop { "max",            max           },
                    prop { "sparse",         sparse        } /// these are overlays we use to make a copied buffer instance with changes made to it
                };
            }

            size_t vcount() {
                size_t vsize = 0;
                switch (type) {
                    case gltf::CompoundType::SCALAR: vsize = 1; break;
                    case gltf::CompoundType::VEC2:   vsize = 2; break;
                    case gltf::CompoundType::VEC3:   vsize = 3; break;
                    case gltf::CompoundType::VEC4:   vsize = 4; break;
                    case gltf::CompoundType::MAT2:   vsize = 4; break;
                    case gltf::CompoundType::MAT3:   vsize = 9; break;
                    case gltf::CompoundType::MAT4:   vsize = 16; break;
                    default: assert(false);
                }
                return vsize;
            };

            size_t component_size() {
                size_t scalar_sz = 0;
                switch (componentType) {
                    case gltf::ComponentType::BYTE:           scalar_sz = sizeof(u8); break;
                    case gltf::ComponentType::UNSIGNED_BYTE:  scalar_sz = sizeof(u8); break;
                    case gltf::ComponentType::SHORT:          scalar_sz = sizeof(u16); break;
                    case gltf::ComponentType::UNSIGNED_SHORT: scalar_sz = sizeof(u16); break;
                    case gltf::ComponentType::UNSIGNED_INT:   scalar_sz = sizeof(u32); break;
                    case gltf::ComponentType::FLOAT:          scalar_sz = sizeof(float); break;
                    default: assert(false);
                }
                return scalar_sz;
            };
        };

        size_t vcount() {
            return data->vcount();
        };

        size_t component_size() {
            return data->component_size();
        }

        mx_basic(Accessor);
    };

    struct BufferView:mx {
        struct M {
            size_t      buffer;
            size_t      byteLength;
            size_t      byteOffset;
            TargetType  target;
            ///
            properties meta() {
                return {
                    prop { "buffer",       buffer       },
                    prop { "byteLength",   byteLength   },
                    prop { "byteOffset",   byteOffset   },
                    prop { "target",       target       }
                };
            }
        };
        mx_basic(BufferView);
    };

    struct Skin:mx {
        struct M {
            str             name;
            Array<int>      joints; /// references nodes!
            int             inverseBindMatrices; /// buffer-view index of mat44 (before index ordering) (assert data type must be matrix 4x4)
            mx              extras;
            mx              extensions;
            ///
            properties meta() {
                return {
                    prop { "name",                name        },
                    prop { "joints",              joints      },
                    prop { "inverseBindMatrices", inverseBindMatrices },
                    prop { "extensions",          extensions  },
                    prop { "extras",              extras      }
                };
            }
        };
        mx_basic(Skin);
    };

    struct JData;
    struct Node;
    struct Transform:mx {
        struct M {
            JData              *jdata;
            int                 istate;
            m44f                local;
            m44f                local_default;
            int                 iparent;
            Array<int>          ichildren; /// all of these are added into Joints::transforms (as well as root Transforms)
        };

        void multiply(const m44f &m) {
            data->local *= m;
            propagate();
        }

        void set(const m44f &m) {
            data->local = m;
            propagate();
        }

        void set_default() {
            data->local = data->local_default;
            propagate();
        }

        void propagate();

        void operator*=(const m44f &m) {
            multiply(m);
        }

        operator bool() { return data->jdata; } /// useful for parent & null state handling

        mx_basic(Transform);
    };

    struct JData {
        Array<m44f>         states;     /// wgpu::Buffer updated with this information per frame
        Array<Transform>    transforms; /// same identity as joints array in skin
    };

    /// references JData
    void Transform::propagate() {
        static m44f ident(1.0f); /// iparent's istate will always == iparent
        m44f &m = (data->iparent != -1) ? data->jdata->states[data->iparent] : ident;
        data->jdata->states[data->istate] = m * data->local;
        for (int &t: data->ichildren.elements<int>())
            data->jdata->transforms[t].propagate();
    }

    struct Joints:mx {
        memory *copy() const {
            Joints joints;
            joints->states = Array<m44f     >(data->states.len());
            memcpy(joints->states.data<m44f>(), data->states.data<m44f>(), sizeof(m44f) * data->states.len());
            joints->states.set_size(data->states.len());

            /// fix mx::copy()
            joints->transforms = Array<Transform>(data->transforms.len());
            memcpy(joints->transforms.data<Transform>(),
                     data->transforms.data<Transform>(),
                     sizeof(Transform) * data->transforms.len());
            joints->transforms.set_size(data->transforms.len());

            for (Transform &transform: joints->transforms.elements<Transform>()) {
                transform->jdata = joints.data;
            }
            return hold(joints);
        }
        size_t total_size() { return data->states.total_size(); }
        size_t count()      { return data->states.len(); }

        Transform &operator[](size_t joint_index) {
            return data->transforms[joint_index];
        }
        mx_object(Joints, mx, JData)
    };

    struct Node:mx {
        struct M {
            str             name;
            int             skin        = -1; /// armature index
            int             mesh        = -1; /// mesh index; this is sometimes not set when there are children referenced and its a group-only
            Array<float>    translation = { 0.0, 0.0, 0.0 };
            Array<float>    rotation    = { 0.0, 0.0, 0.0, 1.0 };
            Array<float>    scale       = { 1.0, 1.0, 1.0 };
            Array<float>    weights     = { }; /// no weights, for no vertices
            Array<int>      children;
            int             joint_index = -1;
            bool            processed;
            bool test;

            ///
            properties meta() {
                return {
                    prop { "name",          name        },
                    prop { "mesh",          mesh        },
                    prop { "skin",          skin        }, /// armature index: Models::skins 
                    prop { "children",      children    },
                    prop { "translation",   translation }, /// apply first
                    prop { "rotation",      rotation    }, /// apply second
                    prop { "scale",         scale       }, /// apply third [these are done on load after json is read in; this is only done in cases where ONE of these are set (we want to set defaults of 1,1,1 for scale, 0,0,0 for translate, etc)]
                    prop { "weights",       weights     }
                };
            }
        };

        mx_basic(Node);
    };

    struct Primitive:mx {
        struct M {
            map          attributes; /// Vertex Attribute Type -> index into accessor
            size_t           indices;
            int              material = -1;
            Mode             mode;
            Array<map>   targets; /// (optional) accessor id for attribute name
            ///
            properties meta() {
                return {
                    prop { "attributes",    attributes },
                    prop { "indices",       indices    },
                    prop { "material",      material   },
                    prop { "mode",          mode       },
                    prop { "targets",       targets    }
                };
            }
        };
        mx_basic(Primitive);
    };

    struct MeshExtras:mx {
        struct M {
            Array<str>       target_names;
            ///
            properties meta() {
                return {
                    prop { "targetNames",  target_names }
                };
            }
        };
        mx_basic(MeshExtras);
    };

    struct Mesh:mx {
        struct M {
            str              name;
            Array<Primitive> primitives;
            Array<float>     weights;
            MeshExtras       extras;
            ///
            properties meta() {
                return {
                    prop { "name",         name       },
                    prop { "primitives",   primitives },
                    prop { "weights",      weights    },
                    prop { "extras",       extras     }
                };
            }
        };
        mx_basic(Mesh);
    };

    struct Scene:mx {
        struct M {
            str              name;
            Array<size_t>    nodes;
            ///
            properties meta() {
                return {
                    prop { "name",   name  },
                    prop { "nodes",  nodes }
                };
            }
        };
        mx_basic(Scene);
    };

    struct AssetDesc:mx {
        struct M {
            str generator;
            str copyright;
            str version;
            ///
            properties meta() {
                return {
                    prop { "generator", generator },
                    prop { "copyright", copyright },
                    prop { "version",   version   }
                };
            }
        };
        mx_basic(AssetDesc);
    };

    struct Buffer:mx {
        struct M {
            size_t    byteLength;
            Array<u8> uri;
            ///
            properties meta() {
                return {
                    prop { "byteLength", byteLength },
                    prop { "uri",        uri        }
                };
            }
        };
        mx_basic(Buffer);
    };

    struct Model:mx {
        struct M {
            Array<Node>       nodes;
            Array<Skin>       skins;
            Array<Accessor>   accessors;
            Array<BufferView> bufferViews;
            Array<Mesh>       meshes;
            size_t            scene; // default scene
            Array<Scene>      scenes;
            AssetDesc         asset;
            Array<Buffer>     buffers;
            Array<Animation>  animations;
            ///
            properties meta() {
                return {
                    prop { "nodes",       nodes },
                    prop { "skins",       skins },
                    prop { "accessors",   accessors },
                    prop { "bufferViews", bufferViews },
                    prop { "meshes",      meshes },
                    prop { "scene",       scene },
                    prop { "scenes",      scenes },
                    prop { "asset",       asset },
                    prop { "buffers",     buffers },
                    prop { "animations",  animations }
                };
            }
        };
        mx_basic(Model);

        static Model load(path p) { return p.read<Model>(); }

        Node *find(str name) {
            for (Node &node: data->nodes.elements<Node>()) {
                if (node->name != name) continue;
                return &node;
            }
            return (Node*)null;
        }

        Node &operator[](str name) { return *find(name); }

        Joints joints(Node &node) {
            Joints joints;
            
            if (node->skin != -1) {
                Skin &skin = data->skins[node->skin];

                ion::uniques<int> all_children; /// pre-alloc'd at the size of nodes array
                for (int &node_index: skin->joints.elements<int>()) {
                    Node &node = data->nodes[node_index];
                    for (int i: node->children.elements<int>()) {
                        assert(!all_children.contains(i));
                        all_children.set(i);
                    }
                }

                int j_index = 0;
                for (int &node_index: skin->joints)
                    data->nodes[node_index]->joint_index = j_index++;

                joints->transforms = Array<Transform>(skin->joints.len()); /// we are appending, so dont set size (just verify)
                joints->states     = Array<m44f     >(skin->joints.len());
                joints->states    .set_size(skin->joints.len());
                Transform null;
                /// for each root joint, resolve the local and global matrices
                for (int &node_index: skin->joints) {
                    if (all_children.contains(node_index))
                        continue;
                    
                    m44f      ident = m44f(1.0f);
                    node_transform(joints, ident, node_index, null);
                }
            }
            /// test code!
            for (m44f &state: joints->states) {
                state = m44f(1.0f);
            }

            /// adding transforms twice
            assert(joints->states.len() == joints->transforms.len());
            return joints;
        }

        protected:

        /// builds Transform, separate from Model/Skin/Node and contains usable glm types
        Transform node_transform(Joints joints, const m44f &parent_mat, int node_index, Transform &parent) {
            Node &node = data->nodes[node_index];
            assert(node->processed == false);

            node->processed = true;
            Transform transform;
            if (node->joint_index >= 0) {
                transform->jdata     = joints.data;
                transform->local     = m44f(1.0f);
                
                quatf q(
                    node->rotation[3], node->rotation[0],
                    node->rotation[1], node->rotation[2]);
                
                transform->local     = translate(transform->local, vec3f(node->translation[0], node->translation[1], node->translation[2]));
                transform->local    *= m44f(q);
                transform->local     = scale    (transform->local, vec3f(node->scale[0], node->scale[1], node->scale[2]));
                transform->local_default = transform->local;
                transform->iparent   = parent->istate;
                transform->istate    = node->joint_index;
                m44f      &state_mat = joints->states[transform->istate] = parent_mat * transform->local_default;

                for (int node_index: node->children) {
                    /// ch is referenced from the ops below, when called here (not released)
                    Transform ch = node_transform(joints, state_mat, node_index, transform);
                    if (ch) {
                        assert(data->nodes[node_index]->joint_index == ch->istate);
                        transform->ichildren += ch->istate; /// there are cases where a referenced node is not part of the joints array; we dont add those.
                    }
                }

                joints->transforms += transform;
            }
            return transform;
        }

    };
};

};