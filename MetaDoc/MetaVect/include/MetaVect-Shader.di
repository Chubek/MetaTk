module MetaVect_Shader;
/// D interface counterpart for the shader public API.
enum abiVersion = 1;
struct ShaderDescriptor { string name; string description; uint capabilities; }
