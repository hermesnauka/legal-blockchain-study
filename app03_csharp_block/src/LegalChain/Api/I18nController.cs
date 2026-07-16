using System.Collections.Concurrent;
using System.Text.Json;
using Microsoft.AspNetCore.Mvc;

namespace LegalChain.Api;

/// <summary>
/// Server-side dictionaries backing the mandatory PL/EN language switcher: the frontend
/// keeps its own UI dictionaries, and this endpoint serves the shared, backend-owned
/// strings (consensus/PQC descriptions) so both nodes present identical wording.
/// </summary>
[ApiController]
public class I18nController : ControllerBase
{
    private static readonly HashSet<string> Supported = ["en", "pl"];

    private readonly IWebHostEnvironment _env;
    private readonly ConcurrentDictionary<string, Dictionary<string, string>> _cache = new();

    public I18nController(IWebHostEnvironment env)
    {
        _env = env;
    }

    [HttpGet("/api/i18n/{lang}")]
    public Dictionary<string, string> Messages(string lang)
    {
        string normalized = lang.ToLowerInvariant();
        if (!Supported.Contains(normalized))
        {
            throw new ArgumentException("Unsupported language: " + lang + " (supported: en, pl)");
        }
        return _cache.GetOrAdd(normalized, Load);
    }

    private Dictionary<string, string> Load(string lang)
    {
        string path = Path.Combine(_env.ContentRootPath, "i18n", "messages_" + lang + ".json");
        using var stream = System.IO.File.OpenRead(path);
        return JsonSerializer.Deserialize<Dictionary<string, string>>(stream)
               ?? throw new InvalidOperationException("Missing i18n bundle for " + lang);
    }
}
