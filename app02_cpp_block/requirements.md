
# ROLE 1: ACT AS THE ENTERPRISE SYSTEM ARCHITECT & PRODUCT OWNER (PO)

## GOAL
Your task is to prepare a comprehensive system architecture, requirements specifications (Frontend and Backend), as well as Epics and User Stories for a compliant, educational, and functional application based on Blockchain 3.0, NFT, and Smart Contracts. The application is to serve as an MVP (Minimum Viable Product) for an advanced, decentralized system built on C++ Cpluplus Cpp.
The application must have a switch to display it in Polish or English, using a simple switcher with a flag.

## TASKS TO BE COMPLETED IN THIS STEP:
1. **User Stories & Epics:** Define professional User Stories (in the format: *As a [role], I want to [action], so that [benefit]*), covering:
   - A functional, compliant general ledger registry (blockchain).
   - Creation and certification of digital ownership (NFT) and creator identity (SSI - Self-Sovereign Identity).
   - Crowdfunding mechanisms.
   - Ledger synchronization for two remote users with privacy preservation (Zero-Knowledge Proofs).
2. **Backend Requirements (C++ Cpluplus Cpp):** Describe the implementation architecture of post-quantum cryptography (NIST PQC, QKD), node management, consensus algorithms, and a Smart Contract engine for medicine/agriculture.
3. **Frontend Requirements:** Design the architecture of an educational module connected with live code. The frontend must visualize: blocks, chains, transactions, nodes, tokenomics, and explain consensus types (PoW, PoS, Hybrid, DPoS, PoReputation, PoUtility, BFT, PoH, PoET).
4. **Connectors & Skills:** List the required integration connectors (e.g., WebSockets for P2P, BouncyCastle PQC provider, IPFS Connector for NFT metadata, REST/GraphQL) and "Skills" (AI/Agent capabilities) needed for implementation (e.g., `quantum-crypto-engineering`, `spring-boot-ledger-management`, `react-dapp-visualization`).

## OUTPUT FORMAT
Use professional software engineering language (descriptive UML, Markdown). Focus on compliance, transparency, and cybersecurity. Conclude this step with a ready-to-implement requirements list for the code execution in the next step.
The application must have a switch to display it in Polish or English, using a simple switcher with a flag.


#


# ROLE 2: ACT AS THE LEAD C++ Cpluplus Cpp BLOCKCHAIN, CRYPTOGRAPHY & BACKEND ENGINEER

## GOAL
Based on the approved architecture, implement the foundations of a compliant distributed ledger (Blockchain 3.0) in C++ Cpluplus Cpp. The code must function as an actual general ledger, not just a mock.
The application must have a switch to display it in Polish or English, using a simple switcher with a flag.

## IMPLEMENTATION REQUIREMENTS (CODE TO BE WRITTEN):
1. **Blockchain Core & Ledger:** Create data structures for Blocks and Chain. Implement the mechanism of adding blocks and rewards (tokenomics). 
2. **Post-Quantum Cryptography (PQC):** Implement security against quantum computers. Integrate libraries (e.g., Bouncy Castle PQC) to simulate or implement NIST-compliant algorithms (e.g., Dilithium, Kyber). Apply QKD-inspired (Quantum Key Distribution) mechanisms to secure communication between nodes.
3. **Smart Contracts (Blockchain 3.0):** Implement a compliant Smart Contract interface that can be adapted for:
   - Medical institutions (patient history management based on consent).
   - Agriculture (supply chain transparency).
4. **Consensus and Scalability:** Prepare design patterns (Strategy Pattern) for various consensus algorithms, emphasizing BFT (Byzantine Fault Tolerance) and PoS.
5. **P2P Synchronization:** Implement a mechanism to synchronize the full ledger for 2 remote nodes connected over a network. Use WebSockets or gRPC. Secure the communication channel against Man-In-The-Middle (MITM) attacks and explain in the code comments (Javadoc) how these algorithms ensure trust between parties without revealing their full identities.

## CODING PRINCIPLES
The code must be production-ready, use Java Records, virtual threads (Project Loom), and be strictly documented, emphasizing the explanation of "why this technology is compliant and secure". Code in an optimized manner.
The application must have a switch to display it in Polish or English, using a simple switcher with a flag.


#


# ROLE 3: ACT AS THE LEAD FRONTEND DEVELOPER & TECHNICAL EDUCATOR (LEAD FRONTEND & EDU-TECH DEV)

## GOAL
Your task is to create the user interface layer (React/Vue/Angular - propose the optimal one) that will consume the API from the previously written Spring Boot backend. The interface must be a "live demonstration" of blockchain technology as well as an interactive encyclopedia.
The application must have a switch to display it in Polish or English, using a simple switcher with a flag.

## FRONTEND FUNCTIONAL REQUIREMENTS:
1. **Ledger Visualization (General Ledger):** Create a dynamic view showing block creation, chains, cryptographic hashes, and the P2P network (communication between two remote nodes). 
2. **Educational Section - Consensus Mechanisms:** Implement interactive UI components (e.g., tooltips, accordions, animations) that explain in a detailed and accessible way:
   - Proof of Work (PoW), Proof of Stake (PoS), Hybrid, Proof of Reputation (PoReputation), Proof of Utility (PoUtility), BFT, Proof of History (PoH), Proof of Elapsed Time (PoET).
3. **Educational Section - Cybersecurity and PQC:** Prepare views that visualize how post-quantum cryptography (NIST PQC, QKD) works in this application compared to standard cryptocurrencies (such as Bitcoin). Explain security mechanisms against malicious actors.
4. **Business Use Cases Module:** Create informative and functional dashboards demonstrating the utility and compliance of the implemented solutions (Smart Contracts, NFTs, Tokens) for the following sectors:
   - Creative industries and digital rights management (DRM).
   - Logistics and supply chain (transparency).
   - Medicine and Agriculture.
   - Higher education (credential/diploma verification).
   - Democracy (remote, compliant voting project).
   - Blockchain communities and social enterprise management systems.

## EXPECTED OUTCOME:
Generate the frontend component structure, main view code, and detailed explanatory texts (in English) that will be integrated into the application to explain decentralization, self-sovereign identity, and scalability.
The application must have a switch to display it in Polish or English, using a simple switcher with a flag.
