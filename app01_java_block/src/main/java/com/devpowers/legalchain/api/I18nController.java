package com.devpowers.legalchain.api;

import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.springframework.core.io.ClassPathResource;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RestController;

import java.io.IOException;
import java.io.UncheckedIOException;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Server-side dictionaries backing the mandatory PL/EN language switcher: the frontend
 * keeps its own UI dictionaries, and this endpoint serves the shared, backend-owned
 * strings (consensus/PQC descriptions) so both nodes present identical wording.
 */
@RestController
public class I18nController {

    private static final Set<String> SUPPORTED = Set.of("en", "pl");

    private final ObjectMapper mapper;
    private final Map<String, Map<String, String>> cache = new ConcurrentHashMap<>();

    public I18nController(ObjectMapper mapper) {
        this.mapper = mapper;
    }

    @GetMapping("/api/i18n/{lang}")
    public Map<String, String> messages(@PathVariable String lang) {
        String normalized = lang.toLowerCase();
        if (!SUPPORTED.contains(normalized)) {
            throw new IllegalArgumentException("Unsupported language: " + lang + " (supported: en, pl)");
        }
        return cache.computeIfAbsent(normalized, this::load);
    }

    private Map<String, String> load(String lang) {
        try {
            return mapper.readValue(
                    new ClassPathResource("i18n/messages_" + lang + ".json").getInputStream(),
                    new TypeReference<>() {
                    });
        } catch (IOException e) {
            throw new UncheckedIOException("Missing i18n bundle for " + lang, e);
        }
    }
}
