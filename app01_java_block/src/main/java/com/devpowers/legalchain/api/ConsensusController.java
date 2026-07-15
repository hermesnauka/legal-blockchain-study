package com.devpowers.legalchain.api;

import com.devpowers.legalchain.consensus.ConsensusEngine;
import com.devpowers.legalchain.core.LedgerEvent;
import jakarta.validation.Valid;
import jakarta.validation.constraints.NotBlank;
import org.springframework.context.ApplicationEventPublisher;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.Map;

/** Inspect and switch the active consensus strategy (Strategy pattern, live). */
@RestController
@RequestMapping("/api/consensus")
public class ConsensusController {

    private final ConsensusEngine engine;
    private final ApplicationEventPublisher events;

    public ConsensusController(ConsensusEngine engine, ApplicationEventPublisher events) {
        this.engine = engine;
        this.events = events;
    }

    @GetMapping
    public Map<String, Object> consensus() {
        return Map.of(
                "active", engine.active().name(),
                "available", engine.available().stream()
                        .map(s -> Map.of("name", s.name(), "description", s.description()))
                        .toList());
    }

    @PostMapping
    public Map<String, Object> switchStrategy(@Valid @RequestBody SwitchRequest request) {
        engine.switchTo(request.strategy());
        events.publishEvent(new LedgerEvent(LedgerEvent.Type.CONSENSUS_CHANGED,
                Map.of("active", engine.active().name())));
        return Map.of("active", engine.active().name());
    }

    public record SwitchRequest(@NotBlank String strategy) {
    }
}
