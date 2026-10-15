#0.10.1 beta — scope and validation

Final in-scope review found no additional release-blocking implementation defect.
The user accepted only the legacy pre-new-code stock-return power interruption
limit. No direct Gate migrator or resident modification was added. No physical
LCD/RF/power-cut test was performed; software coverage is not hardware assurance.

| Requirement | Evidence |
|---|---|
| B9 menus / Android11, A English |348 Product entries;1,029 resolved Android keys per locale; pack identity and unsupported branches reviewed |
| English unchanged / selected language only |22 identical English command/RAM_G frames;198 identical optimization frames;116 runtime cases per optimization |
| Titles N / items R / Japanese pack / menu layout |261 final remaining-text frames;383 Japanese menu scalars present at24/32px; existing system/detail captures |
| CJ + emoji repertoire and licenses |32,564 glyph records,6,498 sequences; pinned Noto Emoji3.002; archive hashes/licenses and required repertoire audits |
| Separate phone/device text, newest content |43 ARM text cases per optimization;49 actual Android-to-ARM cases;76 focused Android media/import/reconnect tests |
| FONT diagnosis and English fallback |225 service cases per optimization;12 required files; healthy/no-write and structural-only repair cases |
| Bounded allocation, COW and resume |14 planner,148 transfer,853 publication,312 transaction and179 transaction-edge cases per optimization; media/lease and board tests |
| Confirm Product before resources |28 focused language/install cases; actual archive import; final46 installer/stock-return/manual-resume/import cases, no skips |
| Boot validates through Gate |46 Gate paths,2 reset entries; actual final Product hash;5 followups with strict physical-zero NOR guard |
| New idle repair preserves active requests |10,288 metadata assertions per O0/Os;4 completion prefixes,140 partial-bit repair cuts,5 Bootstrap intent cases |
| Capacity and retained memory/assets |Debug/Release392,180B used,1,036B free, minimum1,024B; SRAM43,272B free, CCM16,320B; existing heap/stack/UI assets unchanged |
| Matching APK/ZIP/pack |APKcode104, old signer retained, exact Product hash, final Gate/Bootstrap hashes and resource pins checked |
| Relink distribution |353 matching application classes,14 JARs; D8/reconstruction/alignment/signature checks; final kit only changes README text |

Review directly checked the production resource flow: confirmed target/UID/Gate,
stationary lease, fresh probe after user choice, file/hash-bound repair, only
complete verified publication, sticky English-only preferences, boot-latched
language and no UI-thread file reads. Software source snapshots match the live
source; release hashes bind the candidate artifacts. Historical test reports
remain historical; counts above identify the final relevant focused checks.

Known exception: actual released0.9.73 Gate/Product plus saved0.14 resident
reproduces Wait(4) after an interrupted stock-return metadata request, before
new Bootstrap/Gate can execute. The new post-copy Wait(97) issue is fixed, but
this old interval cannot be retroactively repaired. See INSTALL.md.
