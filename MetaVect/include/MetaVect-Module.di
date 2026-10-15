module MetaVect_Module;
/// D interface counterpart for the module public API.
enum abiVersion = 1;
struct ModuleDescriptor { string name; string description; uint capabilities; }
