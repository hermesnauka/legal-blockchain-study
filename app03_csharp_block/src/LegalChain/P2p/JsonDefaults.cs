using System.Text.Json;
using System.Text.Json.Serialization;

namespace LegalChain.P2p;

/// <summary>
/// The single JSON dialect of the node — camelCase properties, UPPER_SNAKE enum names —
/// shared by the REST API, the <c>/ws/events</c> feed and the P2P protocol so that every
/// serialized <c>Block</c>/<c>Transaction</c> matches API contract v1 byte-for-byte.
/// </summary>
public static class JsonDefaults
{
    public static readonly JsonSerializerOptions Options = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = true,
        Converters = { new JsonStringEnumConverter() }
    };
}
