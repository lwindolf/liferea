<?xml version="1.0"?> 

<!--
/**
 * Localization stylesheet for Liferea
 *
 * Copyright (C) 2006 Aristotle Pagaltzis <pagaltzis@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
--> 

<xsl:stylesheet
	version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<!-- the locale to use -->
<xsl:param name="lang" />	<!-- e.g. "de_AT" -->
<xsl:param name="shortlang" />	<!-- e.g. "de" -->

<!-- identity copy -->
<xsl:template match="node()|@*|comment()|processing-instruction()">
	<xsl:copy>
		<xsl:apply-templates select="@*|node()"/>
	</xsl:copy>
</xsl:template>

<!-- drop elements that have an @xml:lang attribute but with 
	 the wrong value -->
<xsl:template match="*[ @xml:lang ]">
	<xsl:if test="@xml:lang = $lang or @xml:lang = $shortlang">
		<xsl:copy>
			<xsl:apply-templates select="@*|node()"/>
		</xsl:copy>
	</xsl:if>
</xsl:template>

<xsl:key
	name="translations"
	match="*[ @xml:lang ]"
	use="preceding-sibling::*[ not( @xml:lang ) ][1]"
	/>

<!-- handling for the untranslated nodes (can be dropped if there 
	 is a node with a translation) -->
<xsl:template match="*[ not( @xml:lang ) ]">
	<xsl:if test="not( key( 'translations', . )[ @xml:lang = $lang or @xml:lang = $shortlang ] )">
		<xsl:copy>
			<xsl:apply-templates select="@*|node()"/>
		</xsl:copy>
	</xsl:if>
</xsl:template>

</xsl:stylesheet>
