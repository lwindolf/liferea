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

var template;

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

	template = Handlebars.compile(document.getElementById('template').innerHTML);
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

async function load_node(data, baseURL, direction) {
	let node = JSON.parse(decodeURIComponent(data));
	let publisher	= metadata_get(node, "publisher");
	let author		= metadata_get(node, "author");
	let copyright	= metadata_get(node, "copyright");
	let description	= metadata_get(node, "description");
	let homepage	= metadata_get(node, "homepage");

	// FIXME
	console.log(node);

	prepare(baseURL, node.title);
	document.body.innerHTML = template({
		node,
		direction,
		publisher  		: metadata_get(node, "publisher"),
		author			: metadata_get(node, "author"),
		copyright		: metadata_get(node, "copyright"),
		description		: metadata_get(node, "description"),
		homepage		: metadata_get(node, "homepage"),
		errorDetails	: (() => {
			if (!node.error)
				return null;

			return `<span>There was a problem when fetching this subscription!</span>
				<ul>
					<li>
						${parseStatus(1, node.error)} <span>1. Authentication</span>
					</li>
					<li>
						${parseStatus(2, node.error)} <span>2. Download</span>
					</li>
					<li>
						${parseStatus(4, node.error)} <span>3. Feed Discovery</span>
					</li>
					<li>
						${parseStatus(8, node.error)} <span>4. Parsing</span>
					</li>
				</ul>

				<span class="details">
				<b><span>Details:</span></b>

					${(1 == node.error)?`
						<p><span>Authentication failed. Please check the credentials and try again!</span></p>
					`:''}
					${(2 == node.error)?`
						${node.httpError?`
							<p>
							${node.httpErrorCode >= 100?`HTTP ${node.httpErrorCode}: `:''}<br/>
							${node.httpError}
							</p>
						`:''}

						${node.updateError?`
							<p>
							<span>There was an error when downloading the feed source:</span>
							<pre class="errorOutput">${node.updateError}</pre>
							</p>
						`:''}

						${node.filterError?`
							<p>
							<span>There was an error when running the feed filter command:</span>
							<pre class="errorOutput">${node.filterError}</pre>
							</p>
						`:''}
					`:''}
					${(4 == node.error)?`
						<p><span>The source does not point directly to a feed or a webpage with a link to a feed!</span></p>
					`:''}
					${(8 == node.error)?`
						<p><span>Sorry, the feed could not be parsed!</span></p>
						<pre class="errorOutput">${node.parseError}</pre>
						<p><span>You may want to contact the author/webmaster of the feed about this!</span></p>
					`:''}
				</span>`;
		})()
	});

    contentCleanup ();
}

async function load_item(data, baseURL, direction) {
	let item = JSON.parse(decodeURIComponent(data));
	let richContent = metadata_get(item, "richContent");
	let mediathumb = metadata_get(item, "mediathumbnail");
	let mediadesc = metadata_get(item, "mediadescription");
	let article;

	if (richContent) {
		let shadowDoc = document.implementation.createHTMLDocument();
		shadowDoc.body.innerHTML = richContent;

		article = new Readability(shadowDoc, {charThreshold: 100}).parse();
		if (article) {
			description = article.content;

			// Use rich content from Readability if available and better!
			if (article.content.length > item.description.length) {
				item.description = article.content;
			}
		}
	}

	// FIXME
	console.log(item);

	prepare(baseURL, item.title);
	document.body.innerHTML = template({
		item,
		direction,

		// Using article.title is important, as often the item.title is just a summary
		// or something slightly different. Using the article.title allow for better
		// title duplicate elimination (further below)
		title		: article?article.title:item.title,

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
	}

    // If there are no images and we have a thumbnail, add it
    if (document.querySelectorAll('img').length == 0 && mediathumb) {
		let img = document.createElement('img');
		img.src = mediathumb;
		img.alt = mediadesc;
		document.getElementById('description').prepend(img);
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
	if (youtubeMatch)
		youtube_embed (youtubeMatch[1]);

    contentCleanup ();

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
