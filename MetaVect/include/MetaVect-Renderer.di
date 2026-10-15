module MetaVect_Renderer;
/// D interface counterpart for the renderer public API.
enum abiVersion = 1;
struct RendererDescriptor { string name; string description; uint capabilities; }
