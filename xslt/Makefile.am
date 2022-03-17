
xslt_in_files = item.xml.in \
                feed.xml.in \
                folder.xml.in \
                html-extract.xml.in \
                html5-extract.xml.in \
                reader.xml.in \
                source.xml.in \
                newsbin.xml.in \
                vfolder.xml.in

xslt_files = $(xslt_in_files:.xml.in=.xml) i18n-filter.xslt
xsltdir = $(pkgdatadir)/xslt
xslt_DATA = $(xslt_files)
@INTLTOOL_XML_RULE@

EXTRA_DIST = \
	$(xslt_DATA) $(xslt_in_files)

DISTCLEANFILES = $(xslt_in_files:.xml.in=.xml)
