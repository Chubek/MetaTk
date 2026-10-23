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

void installAll(Session s)
{
    if (s.find_primitive("Metalib.version") !is null) return;
    installString(s); installList(s); installHash(s); installIO(s);
    installBridge(s); installBuffer(s); installTable(s); installURI(s);
    installGraph(s); installJSON(s); installTOML(s); installYAML(s);
    installSExpr(s); installASTBuilder(s); installWalkAST(s); installWalkVisitor(s);
    installVisitorBuilder(s); installFlexBridge(s); installParsec(s); installYACCBridge(s);
    installFrontend(s); installType(s); installModule(s); installLoader(s);
    s.install_primitive("Metalib.version", "Unit -> String", 1,
        (PrimContext c, Value[] a) => make_str(metamlVersion));
}
alias install_all = installAll;
