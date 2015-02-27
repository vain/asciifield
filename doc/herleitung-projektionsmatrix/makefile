# -------------------------------------------------------------------
# "Generic" makefile for pdflatex documents.
#                                               P. Hofmann, 2009-2013
# -------------------------------------------------------------------
#
# What it does:
# =============
#
#  - A gawk snippet will find all included images.
#  - Those are marked as dependencies of the document.
#  - If we know how to build one of the images, do so ...
#  - ... and remember which have been built.
#  - Other images are included as they are.
#  - An external handler called "pdf-viewer" is supposed to open the
#    final documents.
#
#
# Targets:
# ========
#
#  - main: Just build the PDF.
#  - view: Build it and display it.
#  - clean: Remove anything that has been created.
#
#
# Auto-built images:
# ==================
#
#  - SVG -> PDF
#    [inkscape]
#
#    Like graphics from inkscape...
#
#  - DAT/GNU -> [SVG] -> PDF
#    [gnuplot, inkscape]
#
#    Used for gnuplot: .dat contains your data, .gnu contains your
#    script which has to read data from /dev/stdin. Export to an
#    intermediate SVG file.
#
#  - DOT -> [SVG] -> PDF
#    [dot, inkscape]
#
#    Graphs in dot-language. Export to an intermediate SVG file.
#
#  - Sketch 3D -> TeX (TikZ or PSTricks)
#    [sketch]
#
#    Sketches made with "Sketch 3D" are exported as "*.tex" and are to
#    be included with "\input{foo.sk.tex}" (source name: "foo.sk").
#
#


# All you have to set:
TEXFILE = doc.tex


# -------------------------------------------------------------------
# Settings

.PHONY: main
.PHONY: clean
.PHONY: view

PDFFILE = $(TEXFILE:%.tex=%.pdf)
DEPFILE = .depend
# Match every "includegraphics"-command which is not commented out.
# ("fpic" demonstrates how to add alternatives...)
# All filenames are extracted and printed on a single line.
GAWKMAGIC = gawk --re-interval '\
 BEGIN                                                                 \
 {                                                                     \
  P=".*\\\\(includegraphics|fpic|input)[^{]*\\{([^}]*\\..{1,7})\\}.*"  \
 };                                                                    \
 $$0 ~ P                                                               \
 {                                                                     \
  gsub(/^[ \t]+/, "");                                                 \
  if (substr($$0, 1, 1) != "%")                                        \
  {                                                                    \
   printf "%s ", gensub(P, "\\2", "g");                                \
  }                                                                    \
 };' $(TEXFILE)
BUILTFILE = .built


# -------------------------------------------------------------------
# Global

# Our main target is to build the pdf and its notes file.
main: $(PDFFILE) $(PDFFILE).notes

# Build it, then view it.
view: main
	pdf-viewer $(PDFFILE) &

# Remove anything + the produced pdf's.
clean:
	rm -fv $(DEPFILE)
	rm -fv $(PDFFILE)
	rm -fv $(TEXFILE:%.tex=%.log) $(TEXFILE:%.tex=%.toc)
	rm -fv $(TEXFILE:%.tex=%.out) $(TEXFILE:%.tex=%.aux)
	rm -fv $(TEXFILE:%.tex=%.bbl) $(TEXFILE:%.tex=%.blg)
	rm -fv $(TEXFILE:%.tex=%.lof) $(TEXFILE:%.tex=%.lol)
	rm -fv $(TEXFILE:%.tex=%.snm) $(TEXFILE:%.tex=%.nav)
	rm -fv $(TEXFILE:%.tex=%.lot)
	
	@# Only remove those files which we marked as being built by us.
	@# We must use bash for this due to "read -r".
	bash -c '                         \
	if [[ -f "$(BUILTFILE)" ]];       \
	then                              \
		while read -r line;           \
		do                            \
			rm -fv "$$line";          \
		done < "$(BUILTFILE)";        \
	fi                                \
	'
	rm -fv $(BUILTFILE)

# Let gawk find out which external pdf's are included. This file will be
# updated when the tex document is changed.
$(DEPFILE): $(TEXFILE)
	echo -n "$(PDFFILE): " > $(DEPFILE)
	$(GAWKMAGIC) >> $(DEPFILE)
	echo "" >> $(DEPFILE)

# That depfile will be included here. It will mark all included images
# as dependencies. For some of them, we have build rules depending on
# which sources files exist.
include $(DEPFILE)


# -------------------------------------------------------------------
# Generic build rules

%.pdf: %.tex
	pdflatex $<
	pdflatex $<

%.pdf.notes: %.tex
	if grep -q '%# CREATE SLINP NOTES' $<; then gawk '                 \
			/^\\begin{frame}/ {                                        \
				if (o) {                                               \
					print "}}}";                                       \
					print "";                                          \
				}                                                      \
				o = 0;                                                 \
				s++;                                                   \
			}                                                          \
			                                                           \
			/\\pause/ {                                                \
				if (o) {                                               \
					print "}}}";                                       \
					print "";                                          \
				}                                                      \
				o = 0;                                                 \
				s++;                                                   \
			}                                                          \
			                                                           \
			/%#/ {                                                     \
				if (!o) {                                              \
					print "{{{ " s;                                    \
				}                                                      \
				sub(/^[ 	]+%# ?/, "");                              \
				print $$0;                                             \
				o = 1;                                                 \
			}                                                          \
		' $< > $@;                                                     \
		echo $@ >> $(BUILTFILE);                                       \
		sort $(BUILTFILE) -u -o $(BUILTFILE);                          \
	fi

%.svg: %.dot
	@# DOT -> SVG
	dot -Tsvg $< > $(<:%.dot=%.svg)
	@# Note: This is an intermediate file. We need not to mark it as
	@# "built by me", make will delete it by itself. This is OK because
	@# we never explicitly generate SVG files from DOT files.

%.svg: %.dat %.gnu
	@# gnuplot script that reads data from stdin -> SVG -> PDF
	gnuplot $(<:%.dat=%.gnu) < $< > $(<:%.dat=%.svg)
	@# Note: This is an intermediate file. We need not to mark it as
	@# "built by me", make will delete it by itself. This is OK because
	@# we never explicitly generate SVG files from Gnuplot files.

%.pdf_tex: %.svg
	@# SVG -> PDF with external LaTeX
	inkscape                               \
		--without-gui                      \
		--export-pdf=$(@:%.pdf_tex=%.pdf)  \
		--export-latex                     \
		--file=$<
	@# Unfortunately, Inkscape only includes the basename of the file in
	@# the latex document. But we need the exact name that has been
	@# specified in the original tex document.
	sed -i s:`basename $@ _tex`:$(@:%.pdf_tex=%.pdf):g $@

	@# Be sure to mark this file as "built by me".
	echo $@ >> $(BUILTFILE)
	echo $(@:%.pdf_tex=%.pdf) >> $(BUILTFILE)
	sort $(BUILTFILE) -u -o $(BUILTFILE)

%.pdf: %.svg
	@# SVG -> PDF
	inkscape                   \
		--without-gui          \
		--export-pdf=$@        \
		--file=$<
	@# Be sure to mark this file as "built by me".
	echo $@ >> $(BUILTFILE)
	sort $(BUILTFILE) -u -o $(BUILTFILE)

%.sk.tex: %.sk
	@# Sketch 3D -> TeX (TikZ or PSTricks)
	sketch $< > $@
	@# Be sure to mark this file as "built by me".
	echo $@ >> $(BUILTFILE)
	sort $(BUILTFILE) -u -o $(BUILTFILE)

# vim: set ts=4 sw=4 noet :
