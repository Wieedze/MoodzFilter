# UI rules

* The editor contains no audio logic. It reads and writes parameters through the APVTS attachments.
* Visualization data (analyzer, meters, scope) arrives through a lock-free FIFO fed by the audio thread; the UI reads it on a timer.
* The editor can be destroyed and recreated at any time by the host: no important state lives only in the UI.
* Resizable size, scaling tested at 100 %, 150 %, 200 %.
* Embedded fonts, licenses included.
* The look is described in a design document (colors, sizes, components) before being coded.
* `tools snapshot` produces screenshots for review; an agent does not declare the UI finished without a screenshot.
