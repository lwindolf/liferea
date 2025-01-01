// vim: set ts=4 sw=4:
/*
 * @file htmlview.c  htmlview reader mode switching and CSS handling
 *
 * Copyright (C) 2021-2023 Lars Windolf <lars.windolf@gmx.de>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

function setBase(uri) {
	var base = document.createElement("base");
	base.setAttribute("href", uri);
	document.head.appendChild(base);
}

/**
 * loadContent() will be run on each internal / Readability.js rendering
 *
 * This method can be called multiple times for a single rendering, so it
 * needs to be idempotent on what is done. Primary use case for this is
 * when in internal browser + reader mode we first render layout with parameter
 * content being empty, while asynchronously downloading content. Once the
 * download finishes loadContent() is called again with the actual content
 * which is then inserted in the layout.
 *
 * @returns: false if loading with reader failed (true otherwise)
 */
function loadContent(readerEnabled, content) {
	var internalBrowsing = false;

	if (false == readerEnabled) {
		if (document.location.href === 'liferea://') {
			console.log('[liferea] reader mode is off');
			document.body.innerHTML = decodeURIComponent(content);
		} else {
			console.log('[liferea] reader mode off for website');
		}
	}
	if (true == readerEnabled) {
		try {
			console.log('[liferea] reader mode is on');
			var documentClone = document.cloneNode(true);

			// When we are internally browsing than we need basic
			// structure to insert Reader mode content
			if (document.getElementById('content') !== null) {
				internalBrowsing = true;
				console.log('[liferea] adding <div id="content"> for website content');
				document.body.innerHTML += '<div id=\"content\"></div>';
			}

			// Decide where we get the content from
			if (document.location.href === 'liferea://') {
				// Add all content in shadow DOM and split decoration from content
				// only pass the content to Readability.js
				console.log('[liferea] load content passed by variable');
				content = decodeURIComponent(content);
			} else {
				console.log('[liferea] using content from original document');
				content = document.documentElement.innerHTML;
			}

			// Add content to clone doc as input for Readability.js
			documentClone.body.innerHTML = content;

			// When we run with internal URI schema we get layout AND content
			// from variable and split it, apply layout to document
			// and copy content to documentClone
			if (document.location.href === 'liferea://' && documentClone.getElementById('content') != null) {
				documentClone.getElementById('content').innerHTML = '';
				document.body.innerHTML = documentClone.body.innerHTML;
				documentClone.body.innerHTML = content;
				documentClone.body.innerHTML = documentClone.getElementById('content').innerHTML;
			}

			try {
				if (!isProbablyReaderable(documentClone))
					throw "notreaderable";

				// Show the results
				// Kill all foreign styles
				var links = document.querySelectorAll('link');
				for (var l of links) {
					l.parentNode.removeChild(l);
				}
				var styles = document.querySelectorAll('style');
				for (var s of styles) {
					s.parentNode.removeChild(s);
				}

				var article = new Readability(documentClone, {
					charThreshold: 25
				}).parse();

				if (!article)
					throw "noarticle";

				document.getElementById('content').innerHTML = article.content;

			} catch (e) {
				console.log('[liferea] reader mode not possible (' + e + ')! fallback to unfiltered content');
				if (internalBrowsing)
					document.getElementById('content').innerHTML = "Reader mode not possible. Loading URL unfiltered...";	// FIXME: provide good error info
				else
					document.body.innerHTML = content;
				return false;
			}

			// Kill all foreign styles
			var links = document.querySelectorAll('link');
			for (var l of links) {
				l.parentNode.removeChild(l);
			}
			var styles = document.querySelectorAll('style');
			for (var s of styles) {
				s.parentNode.removeChild(s);
			}
		} catch (e) {
			console.log('[liferea] reader mode failed: ' + e);
			if (!internalBrowsing) {
				// Force load original document at top level to get rid of all decoration
				document.documentElement.innerHTML = content;
			} else {
				document.getElementById('content').innerHTML = "Reader mode failed. Loading URL unfiltered...";
				return false;
			}
		}
	}

	// Run DOMPurify
	content = document.getElementById('content').innerHTML;
	document.getElementById('content').innerHTML = DOMPurify.sanitize(content);

	// Fix inline SVG sizes
	const svgMinWidth = 50;
	document.getElementById('content')
		.querySelectorAll('svg')
		.forEach((el) => {
			const h = el.getAttribute('height');
			const w = el.getAttribute('width');
			if(h && w) {
				if(w < svgMinWidth)
					el.parentNode.removeChild(el);
				return;
			}

			const viewbox = el.getAttribute('viewBox');
			if(!viewbox)
				return;

			const size = viewbox.split(/\s+/);
			if(size.length != 4)
				return;

			// Drop smaller SVGs that are usually just layout decorations
			if((size[2] - size[0]) < svgMinWidth) {
				el.parentNode.removeChild(el);
				return;
			}

			// Properly size larger SVGs
			el.width = size[2] - size[0];
			el.heigth = size[3] - size[1];
		});

	// Drop empty elements (to get rid of empty picture/video/iframe divs)
	const emptyRegex = new RegExp("^\s*$");
	document.getElementById('content')
		.querySelectorAll(":only-child")
		.forEach((el) => {
			if(el.innerHTML.length == 1)
				el.parentNode.removeChild(el);
		});

    // Setup audio/video <select> handler
    document.querySelector('#enclosureVideo select')?.addEventListener("change", (e) => {
		document.querySelector('#enclosureVideo video').src = e.target.options[e.target.selectedIndex].value;
		document.querySelector('#enclosureVideo video').play();
    });
    document.getElementById('#enclosureAudio select')?.addEventListener("change", (e) => {
		document.querySelector('#enclosureAudio audio').src = e.target.options[e.target.selectedIndex].value;
		document.querySelector('#enclosureVideo audio').play();
    });

	return true;
}

function youtube_embed(id) {
	var container = document.getElementById(id);
	container.innerHTML = '<iframe width="640" height="480" src="https://www.youtube.com/embed/' + id + '?autoplay=1" frameborder="0" allowfullscreen="1" allow="autoplay; allowfullscreen"></iframe>';

	return false;
}
