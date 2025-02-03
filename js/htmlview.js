// vim: set ts=4 sw=4:
/*
 * @file htmlview.js  html view for node + item display
 *
 * Copyright (C) 2021-2025 Lars Windolf <lars.windolf@gmx.de>
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

import { render, template } from './helpers/render.js';
import DOMPurify from './vendor/purify.min.js';

window.debugflags = 0;

function prepare(baseURL, title) {
	if (title) {
		/* Set title for it to appear in e.g. desktop MPRIS playback controls */
		document.title = title;
	}
	if (baseURL && baseURL !== '(null)') {
		var base = document.createElement("base");
		base.setAttribute("href", baseURL);
		document.head.appendChild(base);
	}
}

// returns first occurence of a given metadata type
function metadata_get(obj, key) {
	var metadata = obj.metadata;

	if (!metadata)
		return null;

	var results = metadata.filter((e) => key in e);
	if (results.length == 0)
		return null;

	return results[0][key];
}

function parseStatus(parsePhase, errorCode) {

	if (errorCode == 0 || errorCode > parsePhase)
		return "✅";
	if (errorCode == parsePhase)
		return "⛔";
	if (parsePhase > errorCode)
		return "⬜";
}

function templateFix(str, data) {
	// sadly libxslt translating the handlebar templates causes
	// attribute escaping and thereby destroying template expressions
	// in attributes, which we need to restore
	return template(str
					.replace(/"%7B%7B/g, "\"{{")
					.replace(/%7D%7D"/g, "}}\"")
					.replace(/%5B/g, "[")
					.replace(/%5D/g, "]"),
					data);
}

function debug(text, obj) {
	if(window.debugflags > 0)
		if (obj)
			console.log(text, obj);
		else
			console.log(text);
}

async function load_node(data, baseURL, direction) {
	let node = JSON.parse(decodeURIComponent(data));

	// FIXME
	debug("node", node);

	prepare(baseURL, node.title);
	render("body", templateFix(document.getElementById('template').innerHTML), {
		node,
		direction,
		publisher  		: metadata_get(node, "publisher"),
		author			: metadata_get(node, "author"),
		copyright		: metadata_get(node, "copyright"),
		description		: metadata_get(node, "description"),
		homepage		: metadata_get(node, "homepage")
	});

    contentCleanup ();
}

async function load_item(data, baseURL, direction) {
	let item = JSON.parse(decodeURIComponent(data));
	let richContent = metadata_get(item, "richContent");
	let mediathumb = metadata_get(item, "mediathumbnail");
	let mediadesc = metadata_get(item, "mediadescription");
	let article;
	let debugfooter = "<hr/>DEBUG:";

	if (richContent) {
		let shadowDoc = document.implementation.createHTMLDocument();
		shadowDoc.body.innerHTML = richContent;

		article = new Readability(shadowDoc, {charThreshold: 100}).parse();
		if (article) {
			// Use rich content from Readability if available and better!
			if (article.content.length > item.description.length) {
				debug("Using Readability content");
				debugfooter += " Readability";
				item.description = article.content;
			}
		}
	}
	
	debug("item", item);
	debug("article", article);

	prepare(baseURL, item.title);
	render("body", templateFix (document.getElementById('template').innerHTML), {
		item,
		direction,

		// Using article.title is important, as often the item.title is just a summary
		// or something slightly different. Using the article.title allow for better
		// title duplicate elimination (further below)
		title			: article?.title?.length > 5?article.title:item.title,

		author			: metadata_get(item, "author"),
		creator			: metadata_get(item, "creator"),
		sharedby		: metadata_get(item, "sharedby"),
		via				: metadata_get(item, "via"),
		slashSection	: metadata_get(item, "slashSection"),
		slashDepartment	: metadata_get(item, "slashDepartment"),
		mediathumb		: mediathumb,
		mediadesc		: mediadesc,
		videos			: item.enclosures.filter((enclosure) => enclosure.mime?.startsWith('video/')),
		audios			: item.enclosures.filter((enclosure) => enclosure.mime?.startsWith('audio/'))
		// FIXME: use this too
		/*let related		= metadata_get(item, "related");
		let point		= metadata_get(item, "point");
		let mediaviews	= metadata_get(item, "mediaviews");
		let ratingavg	= metadata_get(item, "mediastarRatingavg");
		let ratingmax	= metadata_get(item, "mediastarRatingMax");
		let gravatar	= metadata_get(item, "gravatar");*/
	});

	// Title duplicate elimination:
	// Check if there is an element which contains exactly the text from item.title
	let el = Array.from(document.getElementById('content').querySelectorAll('*')).find(el => el.textContent.trim() === item.title);
	if (el) {
		let innermost = el;
		while (innermost.children.length > 0) {
			innermost = Array.from(innermost.children).find(child => child.textContent.trim() === item.title) || innermost;
		}
		innermost.remove();
		debugfooter += " titleDuplicateRemove";
	}

    // If there are no images and we have a thumbnail, add it
    if (document.querySelectorAll('img').length == 0 && mediathumb) {
		let img = document.createElement('img');
		img.src = mediathumb;
		img.alt = mediadesc;
		document.getElementById('description').prepend(img);
		debugfooter += " thumbnailAdd";
    }

	// Convert all lazy-loaded images
	document.querySelectorAll('img[data-src]').forEach((img) => {
		img.src = img.getAttribute('data-src');
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

	let youtubeMatch = item.source.match(/https:\/\/www\.youtube\.com\/watch\?v=([\w-]+)/);
	if (youtubeMatch) {
		youtube_embed (youtubeMatch[1]);
		debugfooter += " youtube";
	}

    contentCleanup ();

	if(window.debugflags > 0)
    	document.body.innerHTML += debugfooter;

    return true;
}

/**
 * Different content cleanup tasks
 */
function contentCleanup() {

	// Run DOMPurify
	let content = document.getElementById('content').innerHTML;
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
	document.getElementById('content')
		.querySelectorAll(":only-child")
		.forEach((el) => {
			if(el.innerHTML.length == 1)
				el.parentNode.removeChild(el);
		});
}

function youtube_embed(id) {
	var container = document.getElementById(id);
	container.innerHTML = '<iframe width="640" height="480" src="https://www.youtube.com/embed/' + id + '?autoplay=1" frameborder="0" allowfullscreen="1" allow="autoplay; allowfullscreen"></iframe>';

	return false;
}

export { load_node, load_item };