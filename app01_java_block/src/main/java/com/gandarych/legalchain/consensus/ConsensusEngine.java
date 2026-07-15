package com.gandarych.legalchain.consensus;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import java.util.List;
import java.util.Map;
import java.util.function.Function;
import java.util.stream.Collectors;

/**
 * Holds the registry of {@link ConsensusStrategy} beans and the currently active one.
 * Spring injects every strategy on the classpath, so a new consensus algorithm becomes
 * available API-wide by simply adding a {@code @Component} implementing the interface.
 */
@Service
public class ConsensusEngine {

    private final Map<String, ConsensusStrategy> strategies;
    private volatile ConsensusStrategy active;

    public ConsensusEngine(List<ConsensusStrategy> available,
                           @Value("${legalchain.consensus.default-strategy:POS}") String defaultStrategy) {
        this.strategies = available.stream()
                .collect(Collectors.toUnmodifiableMap(ConsensusStrategy::name, Function.identity()));
        this.active = require(defaultStrategy);
    }

    public ConsensusStrategy active() {
        return active;
    }

    public ConsensusStrategy byName(String name) {
        return require(name);
    }

    public void switchTo(String name) {
        this.active = require(name);
    }

    public List<ConsensusStrategy> available() {
        return strategies.values().stream()
                .sorted(java.util.Comparator.comparing(ConsensusStrategy::name))
                .toList();
    }

    private ConsensusStrategy require(String name) {
        ConsensusStrategy strategy = strategies.get(name.toUpperCase());
        if (strategy == null) {
            throw new IllegalArgumentException("Unknown consensus strategy: " + name
                    + " (available: " + strategies.keySet() + ")");
        }
        return strategy;
    }
}
