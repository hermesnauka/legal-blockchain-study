using System.Text.Json;

namespace LegalChain.Api;

/// <summary>
/// Uniform 4xx error bodies (<c>{"error": ...}</c>) for rule violations and bad input —
/// the C# analogue of the Java node's <c>@RestControllerAdvice</c> handler.
/// <see cref="ArgumentException"/> (broken contract/ledger rules) maps to 400,
/// <see cref="UnauthorizedAccessException"/> (failed authenticity checks) to 403.
/// </summary>
public class ApiExceptionMiddleware
{
    private readonly RequestDelegate _next;

    public ApiExceptionMiddleware(RequestDelegate next)
    {
        _next = next;
    }

    public async Task InvokeAsync(HttpContext context)
    {
        try
        {
            await _next(context);
        }
        catch (ArgumentException e) when (!context.Response.HasStarted)
        {
            await WriteErrorAsync(context, StatusCodes.Status400BadRequest, e.Message);
        }
        catch (UnauthorizedAccessException e) when (!context.Response.HasStarted)
        {
            await WriteErrorAsync(context, StatusCodes.Status403Forbidden, e.Message);
        }
    }

    private static Task WriteErrorAsync(HttpContext context, int status, string message)
    {
        context.Response.StatusCode = status;
        context.Response.ContentType = "application/json";
        return context.Response.WriteAsync(
            JsonSerializer.Serialize(new Dictionary<string, string> { ["error"] = message }));
    }
}
