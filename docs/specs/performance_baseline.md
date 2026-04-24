# Performance Baseline

## Non-Negotiable Quality Gates

1. **ISR Execution Time:** "All GPIO ISR handlers MUST execute in < 5us. No looping over unconfigured lines."

2. **Bounded Execution:** "The main application poll loop MUST NOT exceed its drain budget (e.g., max 16 messages per poll) to ensure cooperative fairness."

3. **Zero-Heap Policy:** "Dynamic allocation (`malloc`/`free`) is STRICTLY FORBIDDEN after the initial bootstrap phase. All queues and mailboxes must be statically allocated."
