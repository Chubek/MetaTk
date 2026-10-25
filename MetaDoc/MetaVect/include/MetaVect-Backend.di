module MetaVect_Backend;
/// D interface counterpart for the backend public API.
enum abiVersion = 1;
struct BackendDescriptor { string name; string description; uint capabilities; }
