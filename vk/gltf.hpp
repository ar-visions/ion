#pragma once
#include <mx/mx.hpp>

namespace gltf {
    enums(ContentType, NONE,
        NONE = 0,
        BYTE  = 5120, UNSIGNED_BYTE  = 5121,
        SHORT = 5122, UNSIGNED_SHORT = 5123,
        UNSIGNED_INT = 5125, FLOAT = 5126
    );

    enums(StructType, NONE,
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

    struct Accessor:mx {
        struct M {
            size_t       bufferView;
            ContentType  componentType;
            StructType   type;
            size_t       count;
            array<float> min;
            array<float> max;

            doubly<prop> meta() {
                return {
                    { "bufferView",     bufferView    },
                    { "componentType",  componentType },
                    { "count",          count         },
                    { "type",           type          },
                    { "min",            min           },
                    { "max",            max           }
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

    struct Node:mx {
        struct M {
            str             name;
            size_t          mesh; /// mesh index
            array<float>    scale;
            array<float>    translation;

            doubly<prop> meta() {
                return {
                    { "name",          name        },
                    { "mesh",          mesh        },
                    { "scale",         scale       },
                    { "translation",   translation }
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
            array<mx>        targets; /// (optional) accessor id for attribute name

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

    struct Mesh:mx {
        struct M {
            str              name;
            array<Primitive> primitives;
            array<float>     weights;

            doubly<prop> meta() {
                return {
                    { "name",         name       },
                    { "primitives",   primitives },
                    { "weights",      weights    }
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
            str version;
            doubly<prop> meta() {
                return {
                    { "generator", generator },
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
            array<Accessor>   accessors;
            array<BufferView> bufferViews;
            array<Mesh>       meshes;
            size_t            scene; // default scene
            array<Scene>      scenes;
            AssetDesc         asset;
            array<Buffer>     buffers;
            ///
            doubly<prop> meta() {
                return {
                    { "nodes",       nodes },
                    { "accessors",   accessors },
                    { "bufferViews", bufferViews },
                    { "meshes",      meshes },
                    { "scene",       scene },
                    { "scenes",      scenes },
                    { "asset",       asset },
                    { "buffers",     buffers }
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