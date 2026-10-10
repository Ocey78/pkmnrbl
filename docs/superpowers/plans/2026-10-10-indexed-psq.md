# Indexed quantized AOT instruction plan

Continue the user's native cutscene/gameplay task, inline in the existing E
checkout. Preserve all local files and keep the interpreter OFF.

Evidence: the real Save Data A run after14fc2f8 exits on the unsupported indexed
paired quantized load at802A3978 (the emitted diagnostic reports stale802A3970).
The preceding black interval still renders and performs asset decompression.

1. Add original synthetic AOT fixtures for psq_lx/stx/lux/stux, both W values,
   all GQR selectors, zero-base and zero-index distinctions, aliasing, wrapping,
   update preservation and neighboring sentinels. Execute the actual emitted
   C++, with interpreter entry forbidden. Observe failure before editing code.
2. Decode the six-bit extended opcode and indexed W/I fields; emit a scoped
   uint32 effective address and existing load/store helper, then update RA if
   appropriate. Reject reserved-bit and update-RA0 encodings. Keep other paired
   operations unchanged. Report the static failing instruction address.
3. Run synthetic and complete Release/Debug suites, obtain independent review.
4. Re-export locally into an isolated ignored directory, compare with current
   generated files, and copy only attributable AOT changes. Preserve unrelated
   generated runtime differences. Rebuild native title and rerun Save Data A.
5. Update handoff and push only redistributable code/tests/documents.

Encoding checked against GNU binutils and IBM750CL AppendixB. Existing helpers
have separate signed-scale/saturation limitations; this patch does not claim
full quantization or exception accuracy. Test positive scales and in-range
values for the instruction-dispatch milestone. Trace new failures independently.
