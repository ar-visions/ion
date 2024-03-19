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
        TRIANGLE_FAN = 6
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
            doubly<prop> meta() {
                return {
                    { "input",          input },
                    { "output",         output },
                    { "interpolation",  interpolation },
                };
            }
            register(M);
        };
        mx_basic(Sampler);
    };

    struct ChannelTarget:mx {
        struct M {
            size_t          node;
            str             path; /// field name of node
            doubly<prop> meta() {
                return {
                    { "node",           node },
                    { "path",           path }
                };
            }
            register(M);
        };
        mx_basic(ChannelTarget);
    };

    struct Channel:mx {
        struct M {
            size_t          sampler; /// sampler id
            ChannelTarget   target;
            doubly<prop> meta() {
                return {
                    { "sampler",        sampler },
                    { "target",         target }
                };
            }
            register(M);
        };
        mx_basic(Channel);
    };

    struct Animation:mx {
        struct M {
            str             name;
            array<Sampler>  samplers;
            array<Channel>  channels;
            doubly<prop> meta() {
                return {
                    { "name",           name },
                    { "samplers",       samplers },
                    { "channels",       channels }
                };
            }
            register(M);
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
            doubly<prop> meta() {
                return {
                    { "bufferView",     bufferView    },
                    { "componentType",  componentType }
                };
            }
            register(M);
        };
        mx_basic(SparseInfo);
    };

    struct Sparse:mx {
        struct M {
            size_t        count;
            SparseInfo    indices;
            SparseInfo    values;
            ///
            doubly<prop> meta() {
                return {
                    { "count",          count       },
                    { "indices",        indices     },
                    { "values",         values      }
                };
            }
            register(M);
        };
        mx_basic(Sparse);
    };

    struct Accessor:mx {
        struct M {
            size_t        bufferView;
            ComponentType componentType;
            CompoundType  type;
            size_t        count;
            array<float>  min;
            array<float>  max;
            Sparse        sparse;
            ///
            doubly<prop> meta() {
                return {
                    { "bufferView",     bufferView    },
                    { "componentType",  componentType },
                    { "count",          count         },
                    { "type",           type          },
                    { "min",            min           },
                    { "max",            max           },
                    { "sparse",         sparse        } /// these are overlays we use to make a copied buffer instance with changes made to it
                };
            }
            register(M);
        };
        mx_basic(Accessor);
    };

    struct BufferView:mx {
        struct M {
            size_t      buffer;
            size_t      byteLength;
            size_t      byteOffset;
            TargetType  target;
            ///
            doubly<prop> meta() {
                return {
                    { "buffer",       buffer       },
                    { "byteLength",   byteLength   },
                    { "byteOffset",   byteOffset   },
                    { "target",       target       }
                };
            }
            register(M);
        };
        mx_basic(BufferView);
    };

    struct Skin:mx {
        struct M {
            str             name;
            array<int>      joints; /// references the bufferView; we have to make a new one based on this index
            int             inverseBindMatrices; /// buffer-view index of mat44 (before index ordering) (assert data type must be matrix 4x4)
            mx              extras;
            mx              extensions;
            ///
            doubly<prop> meta() {
                return {
                    { "name",                name        },
                    { "joints",              joints      },
                    { "inverseBindMatrices", inverseBindMatrices },
                    { "extensions",          extensions  },
                    { "extras",              extras      }
                };
            }
            register(M);
        };
        mx_basic(Skin);
    };

    struct Node:mx {
        struct M {
            str             name;
            int             skin        = -1; /// armature index
            int             mesh        = -1; /// mesh index; this is sometimes not set when there are children referenced and its a group-only
            array<float>    translation = { 0.0, 0.0, 0.0 };
            array<float>    rotation    = { 0.0, 0.0, 0.0, 1.0 };
            array<float>    scale       = { 1.0, 1.0, 1.0 };
            array<float>    weights     = { }; /// no weights, for no vertices
            array<int>      children;
            ///
            doubly<prop> meta() {
                return {
                    { "name",          name        },
                    { "mesh",          mesh        },
                    { "skin",          skin        }, /// armature index: Models::skins 
                    { "children",      children    },
                    { "translation",   translation }, /// apply first
                    { "rotation",      rotation    }, /// apply second
                    { "scale",         scale       }, /// apply third [these are done on load after json is read in; this is only done in cases where ONE of these are set (we want to set defaults of 1,1,1 for scale, 0,0,0 for translate, etc)]
                    { "weights",       weights     }
                };
            }
            register(M);
        };
        mx_basic(Node);
    };

    struct Primitive:mx {
        struct M {
            map<mx>          attributes; /// Vertex Attribute Type -> index into accessor
            size_t           indices;
            int              material = -1;
            Mode             mode;
            array<map<mx>>   targets; /// (optional) accessor id for attribute name
            ///
            doubly<prop> meta() {
                return {
                    { "attributes",    attributes },
                    { "indices",       indices    },
                    { "material",      material   },
                    { "mode",          mode       },
                    { "targets",       targets    }
                };
            }
            register(M);
        };
        mx_basic(Primitive);
    };

    struct MeshExtras:mx {
        struct M {
            array<str>       target_names;
            ///
            doubly<prop> meta() {
                return {
                    { "targetNames",  target_names }
                };
            }
            register(M);
        };
        mx_basic(MeshExtras);
    };

    struct Mesh:mx {
        struct M {
            str              name;
            array<Primitive> primitives;
            array<float>     weights;
            MeshExtras       extras;
            ///
            doubly<prop> meta() {
                return {
                    { "name",         name       },
                    { "primitives",   primitives },
                    { "weights",      weights    },
                    { "extras",       extras     }
                };
            }
            register(M);
        };
        mx_basic(Mesh);
    };

    struct Scene:mx {
        struct M {
            str              name;
            array<size_t>    nodes;
            ///
            doubly<prop> meta() {
                return {
                    { "name",   name  },
                    { "nodes",  nodes }
                };
            }
            register(M);
        };
        mx_basic(Scene);
    };

    struct AssetDesc:mx {
        struct M {
            str generator;
            str copyright;
            str version;
            ///
            doubly<prop> meta() {
                return {
                    { "generator", generator },
                    { "copyright", copyright },
                    { "version",   version   }
                };
            }
            register(M);
        };
        mx_basic(AssetDesc);
    };

    struct Buffer:mx {
        struct M {
            size_t    byteLength;
            array<u8> uri;
            ///
            doubly<prop> meta() {
                return {
                    { "byteLength", byteLength },
                    { "uri",        uri        }
                };
            }
            register(M);
        };
        mx_basic(Buffer);
    };

    struct Model:mx {
        struct M {
            array<Node>       nodes;
            array<Skin>       skins; /// these are the armatures/bones; why on earth they call it skins is pretty silly; the bones move the model faces, and those faces may or may not be described as skin on the model
            array<Accessor>   accessors;
            array<BufferView> bufferViews;
            array<Mesh>       meshes;
            size_t            scene; // default scene
            array<Scene>      scenes;
            AssetDesc         asset;
            array<Buffer>     buffers;
            array<Animation>  animations;
            ///
            doubly<prop> meta() {
                return {
                    { "nodes",       nodes },
                    { "skins",       skins },
                    { "accessors",   accessors },
                    { "bufferViews", bufferViews },
                    { "meshes",      meshes },
                    { "scene",       scene },
                    { "scenes",      scenes },
                    { "asset",       asset },
                    { "buffers",     buffers },
                    { "animations",  animations }
                };
            }
            register(M);
        };
        mx_basic(Model);

        static Model load(path p) {
            return p.read<Model>();
        }
    };
};

};