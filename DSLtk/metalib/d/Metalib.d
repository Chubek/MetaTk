/// Portable host-module bundle, mirrored by metalib/cpp/Metalib.h.
/// Install before load_stdlib; installation grants no primitive capabilities.
module metalib.d.Metalib;
import MetaML;
import metalib.d.String;
import metalib.d.List;
import metalib.d.Hash;
import metalib.d.IO;
import metalib.d.Bridge;
import metalib.d.Buffer;
import metalib.d.Table;
import metalib.d.URI;
import metalib.d.Graph;
import metalib.d.JSON;
import metalib.d.TOML;
import metalib.d.YAML;
import metalib.d.SExpr;
import metalib.d.ASTBuilder;
import metalib.d.WalkAST;
import metalib.d.WalkVisitor;
import metalib.d.VisitorBuilder;
import metalib.d.FlexBridge;
import metalib.d.Parsec;
import metalib.d.YACCBridge;
import metalib.d.Frontend;
import metalib.d.Type;
import metalib.d.Module;
import metalib.d.Loader;

void installFoundation(Session s)
{
    if (s.find_primitive("String.utf8_length") is null) installString(s);
    if (s.find_primitive("List.take_while") is null) installList(s);
    if (s.find_primitive("Hash.fnv1a") is null) installHash(s);
    if (s.find_primitive("Bridge.kind") is null) installBridge(s);
    if (s.find_primitive("Buffer.create") is null) installBuffer(s);
    if (s.find_primitive("Table.create") is null) installTable(s);
    if (s.find_primitive("Graph.nodes") is null) installGraph(s);
}

void installData(Session s)
{
    installFoundation(s);
    if (s.find_primitive("URI.parse") is null) installURI(s);
    if (s.find_primitive("JSON.parse") is null) installJSON(s);
    if (s.find_primitive("TOML.parse") is null) installTOML(s);
    if (s.find_primitive("YAML.parse") is null) installYAML(s);
    if (s.find_primitive("SExpr.parse") is null) installSExpr(s);
}

void installLanguage(Session s)
{
    installData(s);
    if (s.find_primitive("ASTBuilder.leaf") is null) installASTBuilder(s);
    if (s.find_primitive("WalkAST.preorder") is null) installWalkAST(s);
    if (s.find_primitive("WalkVisitor.walk") is null) installWalkVisitor(s);
    if (s.find_primitive("VisitorBuilder.make") is null) installVisitorBuilder(s);
    if (s.find_primitive("FlexBridge.tokenize") is null) installFlexBridge(s);
    if (s.find_primitive("Parsec.char") is null) installParsec(s);
    if (s.find_primitive("YACCBridge.check") is null) installYACCBridge(s);
    if (s.find_primitive("Frontend.tokens") is null) installFrontend(s);
    if (s.find_primitive("Type.normalize") is null) installType(s);
    if (s.find_primitive("Module.names") is null) installModule(s);
}

void installIOBundle(Session s)
{
    installFoundation(s);
    if (s.find_primitive("IO.flush") is null) installIO(s);
    if (s.find_primitive("Loader.load") is null) installLoader(s);
}

alias install_foundation = installFoundation;
alias install_data = installData;
alias install_language = installLanguage;
alias install_io = installIOBundle;

void installAll(Session s)
{
    installLanguage(s);
    installIOBundle(s);
    if (s.find_primitive("Metalib.version") !is null) return;
    s.install_primitive("Metalib.version", "Unit -> String", 1,
        (PrimContext c, Value[] a) => make_str(metamlVersion));
}
alias install_all = installAll;
