# Licensing and distribution status

Checked 2026-09-08 against tracked source, patches, `CMakeLists.txt`, bootstrap,
package tooling, ignored upstream notices and primary upstream license sources.
This is an engineering inventory and release decision record, not a legal
clearance opinion. [Third-party notices](../THIRD_PARTY_NOTICES.md) contain exact
revisions, attributions and retained license texts.

## Private staging

WumpaForge is being staged as a private development repository. No blanket
proprietary license, public-source release, commercial distribution permission,
or game-rights grant is asserted. Repository visibility controls access; it does
not change third-party terms. Private modification generally does not require
public disclosure under the GPL; giving copies to other recipients can trigger
distribution obligations even when access is invitation-only. See the FSF's
[private changes and NDA explanations](https://www.gnu.org/licenses/gpl-faq.en.html).

Preserve existing file notices and the component license texts. Do not apply a
repository-wide no-copy, no-modification, no-redistribution, or source-withholding
restriction to code whose license already grants those rights. A future custom
license can cover only material the project actually has authority to license.
Authorship and third-party compatibility of currently unmarked contributions
still need review; omitting a root license does not erase the existing GPL grant.

The [repository content audit](REPOSITORY-AUDIT.md) is a separate check of tracked
history. Keeping the ISO and extracted files out of Git helps scope the staging
operation; it is not proof that every tracked snippet or future binary is free
of third-party rights.

## Current obstacles to a restrictive binary release

1. **A directly linked file already carries GPL terms.**
   `src/nv2a_vertex.c` explicitly says `GPL-2.0-only OR GPL-3.0-only`, and
   `CMakeLists.txt` compiles it into the native executable. Under the applicable
   GPL, distributing a covered combined work requires the corresponding source
   and license freedoms; charging for copies is allowed, but denying recipients
   redistribution/modification rights is not. A proprietary-only EULA and
   withheld corresponding source are not a supported release plan for this
   current linked implementation. Review the combination and choose a compliant
   release approach, secure necessary alternative permissions, or independently
   replace affected code with verified provenance; deleting its header alone
   does not accomplish that. Sources: [GPL v2 §§2–3,6](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html),
   [GPL v3 §§5–6,10](https://www.gnu.org/licenses/gpl-3.0.en.html),
   [FSF commercial-use FAQ](https://www.gnu.org/licenses/gpl-faq.en.html#DoesTheGPLAllowMoney).

2. **The toolkit is mixed-license and statically linked.**
   xboxrecomp's MIT label has explicit xemu-derived LGPL exceptions. Its CMake
   umbrella links static APU and NV2A archives; the audio patch changes marked
   APU source. A release needs a link map/object inventory to determine what is
   incorporated; an apparently unused subsystem is not an established licensing
   exemption. LGPL distribution can support proprietary callers, subject to its
   conditions: retain notices, provide the required library source/modifications,
   permit debugging modifications and reverse engineering for that purpose, and
   satisfy relinking/replacement requirements. A static package generally needs
   suitable caller objects or source and build instructions, not merely a link
   to upstream. [LGPL v2.1 §§2,6](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html).

3. **Upstream attribution is incomplete/inconsistent.**
   xboxrecomp NOTICE describes LGPL-2.1-or-later, while extracted headers say
   version 2 or later; `nv2a_core.c` and `nv2a_state.h` also claim xemu derivation
   but are absent from its named exception list. The exact extraction commit is
   not recorded there. Preserve these facts and resolve them by a per-file
   provenance review; the separately pinned xemu shader reference must not be
   assumed to be that extraction revision. Some audio references still point to
   `master`. Existing claims of newly written pixel/ADPCM implementations are
   documented evidence, not a completed expression/derivation audit.

4. **Game rights are separate from runtime licenses.**
   The current executable includes ahead-of-time translated retail XBE code and
   requires original assets. Neither MIT/GPL/LGPL licensing of tools nor ownership
   of a supplied disc establishes permission to distribute that game code,
   original SDK material, assets, music, marks, or artwork. Any future sale or
   public release needs an actual rights assessment of its exact contents and
   applicable jurisdiction. No such permission or clearance was found in this
   workspace. A user-supplied-disc design can reduce bundled material but is not
   asserted to resolve all rights questions.

## Concrete release work still required

- Define the intended deliverable: developer tool/source, runtime library, or
  complete game executable. Audit generated C and linked original code as well
  as tracked files; select licensing only after resolving their ownership.
- Record a full dependency/link inventory, exact versions and sources. Current
  Homebrew SDL/libepoxy versions are observed, not reproducibly pinned. Include
  package-internal notices if redistributing dylibs or developer tools; operating
  system frameworks and SDK terms need their own packaging assessment.
- Preserve MIT/BSD permission/disclaimer/attribution notices; mark altered SDL
  source and preserve its zlib notice if distributed. Retain vgmstream's notice
  with copied oracle code. Complete GPL/LGPL corresponding-source and relinking
  materials for the selected binary, rather than treating this inventory as the
  finished compliance bundle.
- Re-review the actual archive, installer, app bundle, source offer and EULA
  before sharing it. The existing `tools/package.py` is a local launch helper:
  it copies the binary and symlinks local assets, without bundling notices or
  establishing a distributable, self-contained app.

Adding these notices changes no game behavior and grants no new rights over
upstream or retail material. No build or game launch was performed for this audit.
