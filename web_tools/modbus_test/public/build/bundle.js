
(function(l, r) { if (!l || l.getElementById('livereloadscript')) return; r = l.createElement('script'); r.async = 1; r.src = '//' + (self.location.host || 'localhost').split(':')[0] + ':35729/livereload.js?snipver=1'; r.id = 'livereloadscript'; l.getElementsByTagName('head')[0].appendChild(r) })(self.document);
var app = (function () {
    'use strict';

    function noop() { }
    function add_location(element, file, line, column, char) {
        element.__svelte_meta = {
            loc: { file, line, column, char }
        };
    }
    function run(fn) {
        return fn();
    }
    function blank_object() {
        return Object.create(null);
    }
    function run_all(fns) {
        fns.forEach(run);
    }
    function is_function(thing) {
        return typeof thing === 'function';
    }
    function safe_not_equal(a, b) {
        return a != a ? b == b : a !== b || ((a && typeof a === 'object') || typeof a === 'function');
    }
    function is_empty(obj) {
        return Object.keys(obj).length === 0;
    }
    function append(target, node) {
        target.appendChild(node);
    }
    function insert(target, node, anchor) {
        target.insertBefore(node, anchor || null);
    }
    function detach(node) {
        if (node.parentNode) {
            node.parentNode.removeChild(node);
        }
    }
    function element(name) {
        return document.createElement(name);
    }
    function text(data) {
        return document.createTextNode(data);
    }
    function space() {
        return text(' ');
    }
    function listen(node, event, handler, options) {
        node.addEventListener(event, handler, options);
        return () => node.removeEventListener(event, handler, options);
    }
    function attr(node, attribute, value) {
        if (value == null)
            node.removeAttribute(attribute);
        else if (node.getAttribute(attribute) !== value)
            node.setAttribute(attribute, value);
    }
    function children(element) {
        return Array.from(element.childNodes);
    }
    function set_input_value(input, value) {
        input.value = value == null ? '' : value;
    }
    function select_option(select, value, mounting) {
        for (let i = 0; i < select.options.length; i += 1) {
            const option = select.options[i];
            if (option.__value === value) {
                option.selected = true;
                return;
            }
        }
        if (!mounting || value !== undefined) {
            select.selectedIndex = -1; // no option should be selected
        }
    }
    function select_value(select) {
        const selected_option = select.querySelector(':checked');
        return selected_option && selected_option.__value;
    }
    function custom_event(type, detail, { bubbles = false, cancelable = false } = {}) {
        const e = document.createEvent('CustomEvent');
        e.initCustomEvent(type, bubbles, cancelable, detail);
        return e;
    }

    let current_component;
    function set_current_component(component) {
        current_component = component;
    }

    const dirty_components = [];
    const binding_callbacks = [];
    let render_callbacks = [];
    const flush_callbacks = [];
    const resolved_promise = /* @__PURE__ */ Promise.resolve();
    let update_scheduled = false;
    function schedule_update() {
        if (!update_scheduled) {
            update_scheduled = true;
            resolved_promise.then(flush);
        }
    }
    function add_render_callback(fn) {
        render_callbacks.push(fn);
    }
    // flush() calls callbacks in this order:
    // 1. All beforeUpdate callbacks, in order: parents before children
    // 2. All bind:this callbacks, in reverse order: children before parents.
    // 3. All afterUpdate callbacks, in order: parents before children. EXCEPT
    //    for afterUpdates called during the initial onMount, which are called in
    //    reverse order: children before parents.
    // Since callbacks might update component values, which could trigger another
    // call to flush(), the following steps guard against this:
    // 1. During beforeUpdate, any updated components will be added to the
    //    dirty_components array and will cause a reentrant call to flush(). Because
    //    the flush index is kept outside the function, the reentrant call will pick
    //    up where the earlier call left off and go through all dirty components. The
    //    current_component value is saved and restored so that the reentrant call will
    //    not interfere with the "parent" flush() call.
    // 2. bind:this callbacks cannot trigger new flush() calls.
    // 3. During afterUpdate, any updated components will NOT have their afterUpdate
    //    callback called a second time; the seen_callbacks set, outside the flush()
    //    function, guarantees this behavior.
    const seen_callbacks = new Set();
    let flushidx = 0; // Do *not* move this inside the flush() function
    function flush() {
        // Do not reenter flush while dirty components are updated, as this can
        // result in an infinite loop. Instead, let the inner flush handle it.
        // Reentrancy is ok afterwards for bindings etc.
        if (flushidx !== 0) {
            return;
        }
        const saved_component = current_component;
        do {
            // first, call beforeUpdate functions
            // and update components
            try {
                while (flushidx < dirty_components.length) {
                    const component = dirty_components[flushidx];
                    flushidx++;
                    set_current_component(component);
                    update(component.$$);
                }
            }
            catch (e) {
                // reset dirty state to not end up in a deadlocked state and then rethrow
                dirty_components.length = 0;
                flushidx = 0;
                throw e;
            }
            set_current_component(null);
            dirty_components.length = 0;
            flushidx = 0;
            while (binding_callbacks.length)
                binding_callbacks.pop()();
            // then, once components are updated, call
            // afterUpdate functions. This may cause
            // subsequent updates...
            for (let i = 0; i < render_callbacks.length; i += 1) {
                const callback = render_callbacks[i];
                if (!seen_callbacks.has(callback)) {
                    // ...so guard against infinite loops
                    seen_callbacks.add(callback);
                    callback();
                }
            }
            render_callbacks.length = 0;
        } while (dirty_components.length);
        while (flush_callbacks.length) {
            flush_callbacks.pop()();
        }
        update_scheduled = false;
        seen_callbacks.clear();
        set_current_component(saved_component);
    }
    function update($$) {
        if ($$.fragment !== null) {
            $$.update();
            run_all($$.before_update);
            const dirty = $$.dirty;
            $$.dirty = [-1];
            $$.fragment && $$.fragment.p($$.ctx, dirty);
            $$.after_update.forEach(add_render_callback);
        }
    }
    /**
     * Useful for example to execute remaining `afterUpdate` callbacks before executing `destroy`.
     */
    function flush_render_callbacks(fns) {
        const filtered = [];
        const targets = [];
        render_callbacks.forEach((c) => fns.indexOf(c) === -1 ? filtered.push(c) : targets.push(c));
        targets.forEach((c) => c());
        render_callbacks = filtered;
    }
    const outroing = new Set();
    function transition_in(block, local) {
        if (block && block.i) {
            outroing.delete(block);
            block.i(local);
        }
    }
    function mount_component(component, target, anchor, customElement) {
        const { fragment, after_update } = component.$$;
        fragment && fragment.m(target, anchor);
        if (!customElement) {
            // onMount happens before the initial afterUpdate
            add_render_callback(() => {
                const new_on_destroy = component.$$.on_mount.map(run).filter(is_function);
                // if the component was destroyed immediately
                // it will update the `$$.on_destroy` reference to `null`.
                // the destructured on_destroy may still reference to the old array
                if (component.$$.on_destroy) {
                    component.$$.on_destroy.push(...new_on_destroy);
                }
                else {
                    // Edge case - component was destroyed immediately,
                    // most likely as a result of a binding initialising
                    run_all(new_on_destroy);
                }
                component.$$.on_mount = [];
            });
        }
        after_update.forEach(add_render_callback);
    }
    function destroy_component(component, detaching) {
        const $$ = component.$$;
        if ($$.fragment !== null) {
            flush_render_callbacks($$.after_update);
            run_all($$.on_destroy);
            $$.fragment && $$.fragment.d(detaching);
            // TODO null out other refs, including component.$$ (but need to
            // preserve final state?)
            $$.on_destroy = $$.fragment = null;
            $$.ctx = [];
        }
    }
    function make_dirty(component, i) {
        if (component.$$.dirty[0] === -1) {
            dirty_components.push(component);
            schedule_update();
            component.$$.dirty.fill(0);
        }
        component.$$.dirty[(i / 31) | 0] |= (1 << (i % 31));
    }
    function init(component, options, instance, create_fragment, not_equal, props, append_styles, dirty = [-1]) {
        const parent_component = current_component;
        set_current_component(component);
        const $$ = component.$$ = {
            fragment: null,
            ctx: [],
            // state
            props,
            update: noop,
            not_equal,
            bound: blank_object(),
            // lifecycle
            on_mount: [],
            on_destroy: [],
            on_disconnect: [],
            before_update: [],
            after_update: [],
            context: new Map(options.context || (parent_component ? parent_component.$$.context : [])),
            // everything else
            callbacks: blank_object(),
            dirty,
            skip_bound: false,
            root: options.target || parent_component.$$.root
        };
        append_styles && append_styles($$.root);
        let ready = false;
        $$.ctx = instance
            ? instance(component, options.props || {}, (i, ret, ...rest) => {
                const value = rest.length ? rest[0] : ret;
                if ($$.ctx && not_equal($$.ctx[i], $$.ctx[i] = value)) {
                    if (!$$.skip_bound && $$.bound[i])
                        $$.bound[i](value);
                    if (ready)
                        make_dirty(component, i);
                }
                return ret;
            })
            : [];
        $$.update();
        ready = true;
        run_all($$.before_update);
        // `false` as a special case of no DOM component
        $$.fragment = create_fragment ? create_fragment($$.ctx) : false;
        if (options.target) {
            if (options.hydrate) {
                const nodes = children(options.target);
                // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
                $$.fragment && $$.fragment.l(nodes);
                nodes.forEach(detach);
            }
            else {
                // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
                $$.fragment && $$.fragment.c();
            }
            if (options.intro)
                transition_in(component.$$.fragment);
            mount_component(component, options.target, options.anchor, options.customElement);
            flush();
        }
        set_current_component(parent_component);
    }
    /**
     * Base class for Svelte components. Used when dev=false.
     */
    class SvelteComponent {
        $destroy() {
            destroy_component(this, 1);
            this.$destroy = noop;
        }
        $on(type, callback) {
            if (!is_function(callback)) {
                return noop;
            }
            const callbacks = (this.$$.callbacks[type] || (this.$$.callbacks[type] = []));
            callbacks.push(callback);
            return () => {
                const index = callbacks.indexOf(callback);
                if (index !== -1)
                    callbacks.splice(index, 1);
            };
        }
        $set($$props) {
            if (this.$$set && !is_empty($$props)) {
                this.$$.skip_bound = true;
                this.$$set($$props);
                this.$$.skip_bound = false;
            }
        }
    }

    function dispatch_dev(type, detail) {
        document.dispatchEvent(custom_event(type, Object.assign({ version: '3.59.2' }, detail), { bubbles: true }));
    }
    function append_dev(target, node) {
        dispatch_dev('SvelteDOMInsert', { target, node });
        append(target, node);
    }
    function insert_dev(target, node, anchor) {
        dispatch_dev('SvelteDOMInsert', { target, node, anchor });
        insert(target, node, anchor);
    }
    function detach_dev(node) {
        dispatch_dev('SvelteDOMRemove', { node });
        detach(node);
    }
    function listen_dev(node, event, handler, options, has_prevent_default, has_stop_propagation, has_stop_immediate_propagation) {
        const modifiers = options === true ? ['capture'] : options ? Array.from(Object.keys(options)) : [];
        if (has_prevent_default)
            modifiers.push('preventDefault');
        if (has_stop_propagation)
            modifiers.push('stopPropagation');
        if (has_stop_immediate_propagation)
            modifiers.push('stopImmediatePropagation');
        dispatch_dev('SvelteDOMAddEventListener', { node, event, handler, modifiers });
        const dispose = listen(node, event, handler, options);
        return () => {
            dispatch_dev('SvelteDOMRemoveEventListener', { node, event, handler, modifiers });
            dispose();
        };
    }
    function attr_dev(node, attribute, value) {
        attr(node, attribute, value);
        if (value == null)
            dispatch_dev('SvelteDOMRemoveAttribute', { node, attribute });
        else
            dispatch_dev('SvelteDOMSetAttribute', { node, attribute, value });
    }
    function set_data_dev(text, data) {
        data = '' + data;
        if (text.data === data)
            return;
        dispatch_dev('SvelteDOMSetData', { node: text, data });
        text.data = data;
    }
    function validate_slots(name, slot, keys) {
        for (const slot_key of Object.keys(slot)) {
            if (!~keys.indexOf(slot_key)) {
                console.warn(`<${name}> received an unexpected slot "${slot_key}".`);
            }
        }
    }
    /**
     * Base class for Svelte components with some minor dev-enhancements. Used when dev=true.
     */
    class SvelteComponentDev extends SvelteComponent {
        constructor(options) {
            if (!options || (!options.target && !options.$$inline)) {
                throw new Error("'target' is a required option");
            }
            super();
        }
        $destroy() {
            super.$destroy();
            this.$destroy = () => {
                console.warn('Component was already destroyed'); // eslint-disable-line no-console
            };
        }
        $capture_state() { }
        $inject_state() { }
    }

    /* src\TestIO.svelte generated by Svelte v3.59.2 */

    const file = "src\\TestIO.svelte";

    function create_fragment(ctx) {
    	let div0;
    	let input0;
    	let t0;
    	let input1;
    	let t1;
    	let select0;
    	let option0;
    	let option1;
    	let option2;
    	let t5;
    	let input2;
    	let t6;
    	let select1;
    	let option3;
    	let option4;
    	let t9;
    	let input3;
    	let t10;
    	let input4;
    	let t11;
    	let button;
    	let t13;
    	let div1;
    	let t14;
    	let t15;
    	let mounted;
    	let dispose;

    	const block = {
    		c: function create() {
    			div0 = element("div");
    			input0 = element("input");
    			t0 = space();
    			input1 = element("input");
    			t1 = space();
    			select0 = element("select");
    			option0 = element("option");
    			option0.textContent = "Set";
    			option1 = element("option");
    			option1.textContent = "Publish";
    			option2 = element("option");
    			option2.textContent = "Get";
    			t5 = space();
    			input2 = element("input");
    			t6 = space();
    			select1 = element("select");
    			option3 = element("option");
    			option3.textContent = "Holding";
    			option4 = element("option");
    			option4.textContent = "Coil";
    			t9 = space();
    			input3 = element("input");
    			t10 = space();
    			input4 = element("input");
    			t11 = space();
    			button = element("button");
    			button.textContent = "Submit";
    			t13 = space();
    			div1 = element("div");
    			t14 = text("Result: ");
    			t15 = text(/*result*/ ctx[7]);
    			attr_dev(input0, "placeholder", "IP");
    			add_location(input0, file, 20, 7, 637);
    			attr_dev(input1, "placeholder", "Port");
    			add_location(input1, file, 21, 7, 688);
    			option0.__value = "set";
    			option0.value = option0.__value;
    			add_location(option0, file, 23, 11, 784);
    			option1.__value = "pub";
    			option1.value = option1.__value;
    			add_location(option1, file, 24, 11, 829);
    			option2.__value = "get";
    			option2.value = option2.__value;
    			add_location(option2, file, 25, 11, 878);
    			if (/*action*/ ctx[2] === void 0) add_render_callback(() => /*select0_change_handler*/ ctx[11].call(select0));
    			add_location(select0, file, 22, 7, 743);
    			attr_dev(input2, "placeholder", "Device ID");
    			add_location(input2, file, 27, 7, 937);
    			option3.__value = "Holding";
    			option3.value = option3.__value;
    			add_location(option3, file, 29, 11, 1040);
    			option4.__value = "Coil";
    			option4.value = option4.__value;
    			add_location(option4, file, 30, 11, 1093);
    			if (/*type*/ ctx[4] === void 0) add_render_callback(() => /*select1_change_handler*/ ctx[13].call(select1));
    			add_location(select1, file, 28, 7, 1001);
    			attr_dev(input3, "placeholder", "Offset");
    			add_location(input3, file, 32, 7, 1154);
    			attr_dev(input4, "placeholder", "Value");
    			add_location(input4, file, 33, 7, 1213);
    			add_location(button, file, 34, 7, 1270);
    			add_location(div0, file, 19, 3, 623);
    			add_location(div1, file, 37, 3, 1335);
    		},
    		l: function claim(nodes) {
    			throw new Error("options.hydrate only works if the component was compiled with the `hydratable: true` option");
    		},
    		m: function mount(target, anchor) {
    			insert_dev(target, div0, anchor);
    			append_dev(div0, input0);
    			set_input_value(input0, /*ip*/ ctx[0]);
    			append_dev(div0, t0);
    			append_dev(div0, input1);
    			set_input_value(input1, /*port*/ ctx[1]);
    			append_dev(div0, t1);
    			append_dev(div0, select0);
    			append_dev(select0, option0);
    			append_dev(select0, option1);
    			append_dev(select0, option2);
    			select_option(select0, /*action*/ ctx[2], true);
    			append_dev(div0, t5);
    			append_dev(div0, input2);
    			set_input_value(input2, /*deviceId*/ ctx[3]);
    			append_dev(div0, t6);
    			append_dev(div0, select1);
    			append_dev(select1, option3);
    			append_dev(select1, option4);
    			select_option(select1, /*type*/ ctx[4], true);
    			append_dev(div0, t9);
    			append_dev(div0, input3);
    			set_input_value(input3, /*offset*/ ctx[5]);
    			append_dev(div0, t10);
    			append_dev(div0, input4);
    			set_input_value(input4, /*value*/ ctx[6]);
    			append_dev(div0, t11);
    			append_dev(div0, button);
    			insert_dev(target, t13, anchor);
    			insert_dev(target, div1, anchor);
    			append_dev(div1, t14);
    			append_dev(div1, t15);

    			if (!mounted) {
    				dispose = [
    					listen_dev(input0, "input", /*input0_input_handler*/ ctx[9]),
    					listen_dev(input1, "input", /*input1_input_handler*/ ctx[10]),
    					listen_dev(select0, "change", /*select0_change_handler*/ ctx[11]),
    					listen_dev(input2, "input", /*input2_input_handler*/ ctx[12]),
    					listen_dev(select1, "change", /*select1_change_handler*/ ctx[13]),
    					listen_dev(input3, "input", /*input3_input_handler*/ ctx[14]),
    					listen_dev(input4, "input", /*input4_input_handler*/ ctx[15]),
    					listen_dev(button, "click", /*handleSubmit*/ ctx[8], false, false, false, false)
    				];

    				mounted = true;
    			}
    		},
    		p: function update(ctx, [dirty]) {
    			if (dirty & /*ip*/ 1 && input0.value !== /*ip*/ ctx[0]) {
    				set_input_value(input0, /*ip*/ ctx[0]);
    			}

    			if (dirty & /*port*/ 2 && input1.value !== /*port*/ ctx[1]) {
    				set_input_value(input1, /*port*/ ctx[1]);
    			}

    			if (dirty & /*action*/ 4) {
    				select_option(select0, /*action*/ ctx[2]);
    			}

    			if (dirty & /*deviceId*/ 8 && input2.value !== /*deviceId*/ ctx[3]) {
    				set_input_value(input2, /*deviceId*/ ctx[3]);
    			}

    			if (dirty & /*type*/ 16) {
    				select_option(select1, /*type*/ ctx[4]);
    			}

    			if (dirty & /*offset*/ 32 && input3.value !== /*offset*/ ctx[5]) {
    				set_input_value(input3, /*offset*/ ctx[5]);
    			}

    			if (dirty & /*value*/ 64 && input4.value !== /*value*/ ctx[6]) {
    				set_input_value(input4, /*value*/ ctx[6]);
    			}

    			if (dirty & /*result*/ 128) set_data_dev(t15, /*result*/ ctx[7]);
    		},
    		i: noop,
    		o: noop,
    		d: function destroy(detaching) {
    			if (detaching) detach_dev(div0);
    			if (detaching) detach_dev(t13);
    			if (detaching) detach_dev(div1);
    			mounted = false;
    			run_all(dispose);
    		}
    	};

    	dispatch_dev("SvelteRegisterBlock", {
    		block,
    		id: create_fragment.name,
    		type: "component",
    		source: "",
    		ctx
    	});

    	return block;
    }

    function instance($$self, $$props, $$invalidate) {
    	let { $$slots: slots = {}, $$scope } = $$props;
    	validate_slots('TestIO', slots, []);
    	let ip = '';
    	let port = '';
    	let action = '';
    	let deviceId = '';
    	let type = '';
    	let offset = '';
    	let value = '';
    	let result = '';

    	async function handleSubmit() {
    		// Call backend API or run command here.
    		// For this example, I'll make a simple API call.
    		const response = await fetch(`/run-command?ip=${ip}&port=${port}&action=${action}&deviceId=${deviceId}&type=${type}&offset=${offset}&value=${value}`);

    		$$invalidate(7, result = await response.text());
    	}

    	const writable_props = [];

    	Object.keys($$props).forEach(key => {
    		if (!~writable_props.indexOf(key) && key.slice(0, 2) !== '$$' && key !== 'slot') console.warn(`<TestIO> was created with unknown prop '${key}'`);
    	});

    	function input0_input_handler() {
    		ip = this.value;
    		$$invalidate(0, ip);
    	}

    	function input1_input_handler() {
    		port = this.value;
    		$$invalidate(1, port);
    	}

    	function select0_change_handler() {
    		action = select_value(this);
    		$$invalidate(2, action);
    	}

    	function input2_input_handler() {
    		deviceId = this.value;
    		$$invalidate(3, deviceId);
    	}

    	function select1_change_handler() {
    		type = select_value(this);
    		$$invalidate(4, type);
    	}

    	function input3_input_handler() {
    		offset = this.value;
    		$$invalidate(5, offset);
    	}

    	function input4_input_handler() {
    		value = this.value;
    		$$invalidate(6, value);
    	}

    	$$self.$capture_state = () => ({
    		ip,
    		port,
    		action,
    		deviceId,
    		type,
    		offset,
    		value,
    		result,
    		handleSubmit
    	});

    	$$self.$inject_state = $$props => {
    		if ('ip' in $$props) $$invalidate(0, ip = $$props.ip);
    		if ('port' in $$props) $$invalidate(1, port = $$props.port);
    		if ('action' in $$props) $$invalidate(2, action = $$props.action);
    		if ('deviceId' in $$props) $$invalidate(3, deviceId = $$props.deviceId);
    		if ('type' in $$props) $$invalidate(4, type = $$props.type);
    		if ('offset' in $$props) $$invalidate(5, offset = $$props.offset);
    		if ('value' in $$props) $$invalidate(6, value = $$props.value);
    		if ('result' in $$props) $$invalidate(7, result = $$props.result);
    	};

    	if ($$props && "$$inject" in $$props) {
    		$$self.$inject_state($$props.$$inject);
    	}

    	return [
    		ip,
    		port,
    		action,
    		deviceId,
    		type,
    		offset,
    		value,
    		result,
    		handleSubmit,
    		input0_input_handler,
    		input1_input_handler,
    		select0_change_handler,
    		input2_input_handler,
    		select1_change_handler,
    		input3_input_handler,
    		input4_input_handler
    	];
    }

    class TestIO extends SvelteComponentDev {
    	constructor(options) {
    		super(options);
    		init(this, options, instance, create_fragment, safe_not_equal, {});

    		dispatch_dev("SvelteRegisterComponent", {
    			component: this,
    			tagName: "TestIO",
    			options,
    			id: create_fragment.name
    		});
    	}
    }

    const app = new TestIO({
           target: document.body
       });

    return app;

})();
//# sourceMappingURL=bundle.js.map
