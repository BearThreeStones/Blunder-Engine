using System.Reflection;
using System.Runtime.Loader;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Blunder.ScriptsCatalog;

/// <summary>
/// Scans a built game assembly for concrete Behaviour subclasses and writes
/// <c>.blunder/behaviour_catalog.json</c> for the native editor.
/// </summary>
static class Program
{
    static int Main(string[] args)
    {
        if (args.Length != 2)
        {
            Console.Error.WriteLine(
                "Usage: Blunder.ScriptsCatalog <game.dll> <out.json>");
            return 1;
        }

        string gameDll = Path.GetFullPath(args[0]);
        string outJson = Path.GetFullPath(args[1]);
        if (!File.Exists(gameDll))
        {
            Console.Error.WriteLine($"Game assembly not found: {gameDll}");
            return 1;
        }

        string? apiPath = ResolveApiPath(gameDll);
        if (apiPath == null)
        {
            Console.Error.WriteLine("Blunder.Api.dll not found beside game or tool");
            return 1;
        }

        try
        {
            var alc = new CatalogLoadContext(apiPath);
            Assembly apiAssembly = alc.LoadFromAssemblyPath(apiPath);
            Assembly gameAssembly = alc.LoadFromAssemblyPath(gameDll);
            Type? behaviourBase = apiAssembly.GetType("Blunder.Behaviour");
            if (behaviourBase == null)
            {
                Console.Error.WriteLine("Blunder.Behaviour not found in Blunder.Api");
                return 1;
            }

            var catalogTypes = new List<CatalogType>();

            foreach (Type type in gameAssembly.GetTypes())
            {
                if (type.IsAbstract || !behaviourBase.IsAssignableFrom(type))
                {
                    continue;
                }

                if (type == behaviourBase)
                {
                    continue;
                }

                string? clrName = type.FullName;
                if (string.IsNullOrEmpty(clrName))
                {
                    continue;
                }

                var members = new List<CatalogMember>();
                const BindingFlags flags =
                    BindingFlags.Public | BindingFlags.Instance |
                    BindingFlags.DeclaredOnly;

                foreach (FieldInfo field in type.GetFields(flags))
                {
                    if (TryMapKind(field.FieldType, out string kind))
                    {
                        if (kind == "string" && HasClipNameMark(field))
                        {
                            kind = "clip_name";
                        }

                        members.Add(new CatalogMember(field.Name, kind));
                    }
                }

                foreach (PropertyInfo property in type.GetProperties(flags))
                {
                    if (!property.CanRead || !property.CanWrite ||
                        property.GetIndexParameters().Length != 0)
                    {
                        continue;
                    }

                    if (TryMapKind(property.PropertyType, out string kind))
                    {
                        if (kind == "string" && HasClipNameMark(property))
                        {
                            kind = "clip_name";
                        }

                        members.Add(new CatalogMember(property.Name, kind));
                    }
                }

                members.Sort((a, b) =>
                    string.Compare(a.Name, b.Name, StringComparison.Ordinal));
                catalogTypes.Add(new CatalogType
                {
                    ClrName = clrName,
                    Members = members,
                });
            }

            catalogTypes.Sort((a, b) =>
                string.Compare(a.ClrName, b.ClrName, StringComparison.Ordinal));

            var root = new CatalogRoot { Types = catalogTypes };
            string json = JsonSerializer.Serialize(
                root,
                new JsonSerializerOptions { WriteIndented = true });
            Directory.CreateDirectory(Path.GetDirectoryName(outJson)!);
            File.WriteAllText(outJson, json);
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"ScriptsCatalog failed: {ex}");
            return 1;
        }
    }

    static string? ResolveApiPath(string gameDll)
    {
        string gameDir = Path.GetDirectoryName(gameDll) ?? "";
        string besideGame = Path.Combine(gameDir, "Blunder.Api.dll");
        if (File.Exists(besideGame))
        {
            return besideGame;
        }

        string toolDir = AppContext.BaseDirectory;
        string besideTool = Path.Combine(toolDir, "Blunder.Api.dll");
        if (File.Exists(besideTool))
        {
            return besideTool;
        }

        return null;
    }

    static bool HasClipNameMark(MemberInfo member)
    {
        foreach (CustomAttributeData attribute in member.GetCustomAttributesData())
        {
            string? attrName = attribute.AttributeType.FullName;
            if (attrName == "Blunder.BehaviourClipNameAttribute")
            {
                return true;
            }
        }

        return false;
    }

    static bool TryMapKind(Type type, out string kind)
    {
        kind = "";
        Type underlying = Nullable.GetUnderlyingType(type) ?? type;
        if (underlying == typeof(bool))
        {
            kind = "bool";
            return true;
        }

        if (underlying == typeof(string))
        {
            kind = "string";
            return true;
        }

        if (underlying == typeof(float) || underlying == typeof(double) ||
            underlying == typeof(int) || underlying == typeof(long))
        {
            kind = "number";
            return true;
        }

        return false;
    }

    sealed class CatalogLoadContext : AssemblyLoadContext
    {
        readonly string _apiPath;

        public CatalogLoadContext(string apiPath) : base(isCollectible: true) =>
            _apiPath = apiPath;

        protected override Assembly? Load(AssemblyName assemblyName)
        {
            if (assemblyName.Name == "Blunder.Api")
            {
                return LoadFromAssemblyPath(_apiPath);
            }

            return null;
        }
    }

    sealed class CatalogRoot
    {
        [JsonPropertyName("types")]
        public List<CatalogType> Types { get; set; } = new();
    }

    sealed class CatalogType
    {
        [JsonPropertyName("clr_name")]
        public string ClrName { get; set; } = "";

        [JsonPropertyName("members")]
        public List<CatalogMember> Members { get; set; } = new();
    }

    sealed class CatalogMember
    {
        public CatalogMember(string name, string kind)
        {
            Name = name;
            Kind = kind;
        }

        [JsonPropertyName("name")]
        public string Name { get; set; }

        [JsonPropertyName("kind")]
        public string Kind { get; set; }
    }
}
