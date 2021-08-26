local ffi = require 'ffi'

ffi.cdef([[

enum jc_special_idx {
	JC_SELF = -2,
};

struct jc_info_struct {
	unsigned width;
	unsigned height;

	unsigned data_width;
	unsigned data_height;

	unsigned short block_width;
	unsigned short block_height;
};

struct jc *jc_new(const char *savepath, int w, int h);
int jc_add_image(struct jc *self, const char *path);
bool jc_get_info(struct jc *self, int idx, struct jc_info_struct *info_out);
bool jc_drawimage(struct jc *self, int idx,
	unsigned destX, unsigned destY,
	unsigned srcX, unsigned srcY,
	int width, int height);
bool jc_save_and_free(struct jc *self);

]])

ffi.load('./jcanvas.so', true)

local C = ffi.C

local check_same = function (file1, file2)
	local rv = os.execute([[
	if ! command -v md5sum >/dev/null; then
		md5sum() { md5 "$@"; }
	fi
	sum1=$(convert ]]..file1..[[ RGB:- | md5sum)
	sum2=$(convert ]]..file2..[[ RGB:- | md5sum)
	[ "$sum1" = "$sum2" ]
	]])
	return rv == true or rv == 0
end

local tmp_files = {}
local add_tmp_file = function (...)
	local del = {}
	for _, name in ipairs({...}) do
		if not tmp_files[name] then
			table.insert(tmp_files, name)
			tmp_files[name] = true
		else
			del[#del+1] = name
		end
	end
	if #del > 0 then
		os.execute('exec rm -f -- '..table.concat(del, ' '))
	end
end
local delete_tmp_files = function ()
	os.execute('exec rm -f -- '..table.concat(tmp_files, ' '))
	tmp_files = {}
end

local check_area_equals = function (file1, file2, w, h,
                                    f1x, f1y,
                                    f2x, f2y)
	f1x = f1x or 0
	f1y = f1y or 0
	f2x = f2x or f1x
	f2y = f2y or f1y
	local rv = os.execute([[
	file1=]]..file1..[[;
	file2=]]..file2..[[;
	w=]]..w..[[ h=]]..h..[[;
	f1x=]]..f1x..[[ f1y=]]..f1y..[[;
	f2x=]]..f2x..[[ f2y=]]..f2y..[[;
	if ! command -v md5sum >/dev/null; then
		md5sum() { md5 "$@"; }
	fi
	#convert -crop ${w}x${h}+${f1x}+${f1y} $file1 eq1.bmp &
	#convert -crop ${w}x${h}+${f2x}+${f2y} $file2 eq2.bmp &
	cksum1=$(convert -crop ${w}x${h}+${f1x}+${f1y} $file1 RGB:- | md5sum)
	cksum2=$(convert -crop ${w}x${h}+${f2x}+${f2y} $file2 RGB:- | md5sum)
	#wait
	[ "$cksum1" = "$cksum2" ]
	]])
	return rv == 0 or rv == true
end

local check_md5_equals = function (file1, file2)
	local rv = os.execute([[
	file1=]]..file1..[[;
	file2=]]..file2..[[;
	exec cmp -s "$file1" "$file2"
	#if ! command -v md5sum >/dev/null; then
	#	md5sum() { md5 "$@"; }
	#fi
	#cksum1=$(md5sum <$file1)
	#cksum2=$(md5sum <$file2)
	#[ "$cksum1" = "$cksum2" ]
	]])
	return rv == 0 or rv == true
end

local color = 'gradient:yellow-blue "(" gradient:black-lime -rotate -90 ")" -compose CopyGreen -composite'
local mono = '-seed 1234 xc: +noise Random -monochrome'

for i, enc in ipairs({
	{name='4:4:4', w=8,  h=8,  bleed_h=0, bleed_v=0, img=color},
	{name='4:4:0', w=8,  h=16, bleed_h=0, bleed_v=1, img=color},
	{name='4:2:2', w=16, h=8,  bleed_h=1, bleed_v=0, img=color},
	{name='4:2:0', w=16, h=16, bleed_h=1, bleed_v=1, img=color},
	{name='4:1:1', w=32, h=8,  bleed_h=0, bleed_v=0, img=color},
	{name='4:1:0', w=32, h=16, bleed_h=0, bleed_v=0, img=color},

	{name='1x1',   w=8,  h=8,  bleed_h=0, bleed_v=0, img=mono},
	{name='1x2',   w=8,  h=16, bleed_h=0, bleed_v=0, img=mono},
	{name='1x4',   w=8,  h=32, bleed_h=0, bleed_v=0, img=mono},
	{name='2x1',   w=16, h=8,  bleed_h=0, bleed_v=0, img=mono},
	{name='2x2',   w=16, h=16, bleed_h=0, bleed_v=0, img=mono},
	{name='2x4',   w=16, h=32, bleed_h=0, bleed_v=0, img=mono},
	{name='4x1',   w=32, h=8,  bleed_h=0, bleed_v=0, img=mono},
	{name='4x2',   w=32, h=16, bleed_h=0, bleed_v=0, img=mono},
	{name='4x4',   w=32, h=32, bleed_h=0, bleed_v=0, img=mono},

	{name='1x1 -colorspace rgb',  w=8, h=8, bleed_h=0, bleed_v=0, img=color},
	{name='1x1 -colorspace cmyk', w=8, h=8, bleed_h=0, bleed_v=0, img=color},
}) do

	print('transform ' .. enc.name .. (enc.img == mono and ' (mono)' or ''))

	add_tmp_file('gradient_orig.png', 'gradient.jpg', 'gradient_dec.png')
	os.execute([[
	convert -define png:compression-level=0 -size 160x160 ]]..enc.img..[[ gradient_orig.png
	convert -sampling-factor ]]..enc.name..[[ gradient_orig.png gradient.jpg
	exec convert -define png:compression-level=0 gradient.jpg gradient_dec.png
	]])

	-- fixme
	if not enc.name:find('cmyk') then
		assert(check_area_equals('gradient.jpg', 'gradient_dec.png', 160, 160))
	end

	--
	-- test: save unmodified and compare
	--

	add_tmp_file('out.jpg')
	local out = C.jc_new('out.jpg', -1, -1) assert(out ~= nil)
	assert(0 == C.jc_add_image(out, 'gradient.jpg'))
	assert(C.jc_drawimage(out, 0,
	    0, 0,    -- dx dy
	    0, 0,    -- sx sy
	    -1, -1)) -- w h
	assert(C.jc_save_and_free(out))
	assert(check_area_equals('out.jpg', 'gradient.jpg', 160, 160))

	--
	-- copy a block in the image (smallest possible unit)
	--

	add_tmp_file('out.jpg')
	local out = C.jc_new('out.jpg', -1, -1)
	assert(0 == C.jc_add_image(out, 'gradient.jpg'))
	assert(C.jc_drawimage(out, 0,
	    0, 0,      -- dx dy
	    0, 0,      -- sx sy
	    160, 160)) -- w h
	assert(C.jc_drawimage(out, 0,
	    1*32, 1*32,    -- dx dy
	    3*32, 3*32,    -- sx sy
	    enc.w, enc.h)) -- w h

	--
	-- while we're here: test that the block size is correct and drawimage rejects wrong parameters
	--
	for w = 0, enc.w+1 do
		if w ~= enc.w then -- not going to work
			for h = 0, enc.h+1 do
				assert(not C.jc_drawimage(out, C.JC_SELF, 0, 0, 0, 0, w, h))
			end
		else -- may work
			for h = 1, enc.h+1 do
				if h ~= enc.h then -- won't work
					assert(not C.jc_drawimage(out, C.JC_SELF, 0, 0, 0, 0, w, h))
				else -- will work
					assert(C.jc_drawimage(out, C.JC_SELF, 0, 0, 0, 0, w, h))
				end
			end
		end
	end
	-- off-by-ones
	assert(not C.jc_drawimage(out, 0, enc.w,     0, 0, 0, 160,             160))
	assert(not C.jc_drawimage(out, 0, enc.w, enc.h, 0, 0, 160,             160))
	assert(not C.jc_drawimage(out, 0,     0, enc.h, 0, 0, 160,             160))
	assert(not C.jc_drawimage(out, 0,     0,     0, 0, 0, 160+enc.w,       160))
	assert(not C.jc_drawimage(out, 0,     0,     0, 0, 0, 160,       160+enc.h))
	assert(not C.jc_drawimage(out, 0,     0,     0, 0, 0, 160+enc.w, 160+enc.h))
	assert(not C.jc_drawimage(out, C.JC_SELF, enc.w,     0, 0, 0, 160, 160))
	assert(not C.jc_drawimage(out, C.JC_SELF, enc.w, enc.h, 0, 0, 160, 160))
	assert(not C.jc_drawimage(out, C.JC_SELF,     0, enc.h, 0, 0, 160, 160))
	assert(not C.jc_drawimage(out, C.JC_SELF,     0,     0, 0, 0, 160+enc.w,       160))
	assert(not C.jc_drawimage(out, C.JC_SELF,     0,     0, 0, 0, 160,       160+enc.h))
	assert(not C.jc_drawimage(out, C.JC_SELF,     0,     0, 0, 0, 160+enc.w, 160+enc.h))

	assert(C.jc_save_and_free(out))

	--
	-- check copied area
	--

	local w = enc.w-(enc.bleed_h+enc.bleed_h)
	local h = enc.h-(enc.bleed_v+enc.bleed_v)
	local x = 32+enc.bleed_h
	local y = 32+enc.bleed_v
	local x_orig = (32*3)+enc.bleed_h
	local y_orig = (32*3)+enc.bleed_v
	assert(check_area_equals('out.jpg', 'gradient.jpg', w, h, x, y, x_orig, y_orig))

	--
	-- check that the bleed values are correct
	--

	for i = 1, enc.bleed_v do
		local w = enc.w
		local h = 1
		local x = 32
		local y = 32-i
		assert(not check_area_equals('out.jpg', 'gradient.jpg', w, h, x, y))
	end
	for i = 1, enc.bleed_v do
		local w = enc.w
		local h = 1
		local x = 32
		local y = (32+enc.h+i)-1
		assert(not check_area_equals('out.jpg', 'gradient.jpg', w, h, x, y))
	end

	for i = 1, enc.bleed_h do
		local w = 1
		local h = enc.h
		local x = 32-i
		local y = 32
		assert(not check_area_equals('out.jpg', 'gradient.jpg', w, h, x, y))
	end
	for i = 1, enc.bleed_h do
		local w = 1
		local h = enc.h
		local x = (32+enc.w+i)-1
		local y = 32
		assert(not check_area_equals('out.jpg', 'gradient.jpg', w, h, x, y))
	end

	--
	-- check that the rest of the image is unmodified
	--

	-- Oxxxx
	-- O!xxx
	-- Oxxxx
	-- Oxxxx
	-- Oxxxx
	local w = 32-enc.bleed_h
	local h = 160
	local x = 0
	local y = 0
	assert(check_area_equals('out.jpg', 'gradient.jpg', w, h, x, y))

	-- oOOOO
	-- o!xxx
	-- oxxxx
	-- oxxxx
	-- oxxxx
	local w = 160-32
	local h = 32-enc.bleed_v
	local x = 32
	local y = 0
	assert(check_area_equals('out.jpg', 'gradient.jpg', w, h, x, y))

	-- ooooo
	-- o!xxx
	-- oOxxx
	-- oOxxx
	-- oOxxx
	local w = 32
	local h = 160-32-enc.h-enc.bleed_v
	local x = 32
	local y = 32+enc.h+enc.bleed_v
	assert(check_area_equals('out.jpg', 'gradient.jpg', w, h, x, y))

	-- ooooo
	-- o!OOO
	-- ooOOO
	-- ooOOO
	-- ooOOO
	local w = (32*3)-enc.bleed_h
	local h = 32*4
	local x = 32+enc.w+enc.bleed_h
	local y = 32
	assert(check_area_equals('out.jpg', 'gradient.jpg', w, h, x, y))

	--
	-- copy the block back from the original image and compare
	--

	add_tmp_file('fixed.jpg')
	local out = C.jc_new('fixed.jpg', -1, -1)
	assert(0 == C.jc_add_image(out, 'out.jpg'))
	assert(1 == C.jc_add_image(out, 'gradient.jpg'))
	assert(C.jc_drawimage(out, 0,
	    0, 0,      -- dx dy
	    0, 0,      -- sx sy
	    160, 160)) -- w h
	assert(C.jc_drawimage(out, 1,
	    1*32, 1*32,    -- dx dy
	    1*32, 1*32,    -- sx sy
	    enc.w, enc.h)) -- w h
	assert(C.jc_save_and_free(out))

	assert(check_area_equals('fixed.jpg', 'gradient.jpg', 160, 160))

	--
	-- copy a bigger area and check that its contents are unmodified
	--

	assert(64 % enc.w == 0)
	assert(64 % enc.h == 0)

	add_tmp_file('out.jpg')
	local out = C.jc_new('out.jpg', -1, -1)
	assert(0 == C.jc_add_image(out, 'gradient.jpg'))
	assert(C.jc_drawimage(out, 0,
	    0, 0,      -- dx dy
	    0, 0,      -- sx sy
	    160, 160)) -- w h
	assert(C.jc_drawimage(out, 0,
	    1*32, 1*32, -- dx dy
	    2*32, 2*32, -- sx sy
	    64, 64))    -- w h
	assert(C.jc_save_and_free(out))

	--
	-- check it
	--

	local w = 64-(enc.bleed_h+enc.bleed_h)
	local h = 64-(enc.bleed_v+enc.bleed_v)
	local x = 32+enc.bleed_h
	local y = 32+enc.bleed_v
	local x_orig = (32*2)+enc.bleed_h
	local y_orig = (32*2)+enc.bleed_v
	assert(check_area_equals('out.jpg', 'gradient.jpg', w, h, x, y, x_orig, y_orig))

end

delete_tmp_files()

--
-- test cropping
-- this is fast and easy because the file comes out identical to what you get from jpegtran
--
for i, enc in ipairs({
	{name='4:4:4', w=8,  h=8,  img=color},
	{name='4:4:0', w=8,  h=16, img=color},
	{name='4:2:2', w=16, h=8,  img=color},
	{name='4:2:0', w=16, h=16, img=color},
	{name='4:1:1', w=32, h=8,  img=color},
	{name='4:1:0', w=32, h=16, img=color},

	-- (are the non-1x1 ones even any different?)
	{name='1x1',   w=8,  h=8,  img=mono},
	{name='1x2',   w=8,  h=16, img=mono},
--	{name='1x4',   w=8,  h=32, img=mono},
	{name='2x1',   w=16, h=8,  img=mono},
	{name='2x2',   w=16, h=16, img=mono},
--	{name='2x4',   w=16, h=32, img=mono},
--	{name='4x1',   w=32, h=8,  img=mono},
--	{name='4x2',   w=32, h=16, img=mono},
--	{name='4x4',   w=32, h=32, img=mono},

	{name='1x1 -colorspace rgb',  w=8, h=8, img=color},
	{name='1x1 -colorspace cmyk', w=8, h=8, img=color},
}) do
local realsize = math.max(enc.w, enc.h)*2+1

for cropsize = realsize, 1, -1 do
	print(string.format('crop %dx%d -> %dx%d (%s%s)',
	    realsize, realsize, cropsize, cropsize, enc.name,
	    enc.img == mono and ' mono' or ''))

	add_tmp_file('gradient.jpg', 'gradient_tran.jpg', 'gradient_ours.jpg')
	os.execute([[
	set -e
	convert -define jpeg:optimize-coding=off -sampling-factor ]]..enc.name..[[ -size ]]..realsize..[[x]]..realsize..[[ ]]..enc.img..[[ gradient.jpg
	exec jpegtran -perfect -crop ]]..cropsize..[[x]]..cropsize..[[+0+0 -outfile gradient_tran.jpg gradient.jpg
	]])

	local out = C.jc_new('gradient_ours.jpg', cropsize, cropsize) assert(out ~= nil)
	assert(0 == C.jc_add_image(out, 'gradient.jpg'))

	--
	-- test: drawimage calls that shouldn't pass
	--

	assert(not C.jc_drawimage(out, 0,
	    0, 0,
	    0, 0,
	    0, 0))
	for w = 1, enc.w+1 do
		if w ~= enc.w then
			for h = 1, enc.h+1 do
				if h ~= enc.h then
					assert(not C.jc_drawimage(out, 0,
					    0, 0,
					    0, 0,
					    w, h))
					assert(not C.jc_drawimage(out, 0,
					    0, 0,
					    w, h,
					    enc.w, enc.h))
					assert(not C.jc_drawimage(out, 0,
					    w, h,
					    0, 0,
					    enc.w, enc.h))
				end
			end
		end
	end

	local round_up = function (n, r) return n%r ~= 0 and (n-(n%r))+r or n end

	-- check these while we're here
	local infop = ffi.new('struct jc_info_struct[1]')
	assert(C.jc_get_info(out, C.JC_SELF, infop))
	assert(infop[0].width == cropsize)
	assert(infop[0].height == cropsize)
	assert(infop[0].data_width == round_up(cropsize, enc.w))
	assert(infop[0].data_height == round_up(cropsize, enc.h))
	assert(infop[0].block_width == enc.w)
	assert(infop[0].block_height == enc.h)

	assert(C.jc_get_info(out, 0, infop))
	assert(infop[0].width == realsize)
	assert(infop[0].height == realsize)
	assert(infop[0].data_width == round_up(realsize, enc.w))
	assert(infop[0].data_height == round_up(realsize, enc.h))
	assert(infop[0].block_width == enc.w)
	assert(infop[0].block_height == enc.h)

	assert(C.jc_drawimage(out, 0,
	    0, 0,    -- dx dy
	    0, 0,    -- sx sy
	    -1, -1)) -- w h

	--if i % 2 == 0 then
		assert(C.jc_drawimage(out, C.JC_SELF,
		    0, 0,    -- dx dy
		    0, 0,    -- sx sy
		    -1, -1)) -- w h
	--end

	assert(C.jc_save_and_free(out))

	if enc.img == mono and enc.name:find('x') and enc.name ~= '1x1' then
		assert(check_area_equals('gradient_tran.jpg', 'gradient_ours.jpg', cropsize, cropsize))
		assert(not check_md5_equals('gradient_tran.jpg', 'gradient_ours.jpg'))

		--
		-- jpegtran converts these to 1x1 sampling so the file md5 isn't identical
		-- the contents do match though
		--

		-- check gradient_ours.jpg
		add_tmp_file('checktmp.jpg')
		local out = C.jc_new('checktmp.jpg', -1, -1) assert(out ~= nil)
		assert(0 == C.jc_add_image(out, 'gradient_ours.jpg'))

		assert(C.jc_get_info(out, 0, infop))
		assert(infop[0].width == cropsize)
		assert(infop[0].height == cropsize)
		assert(infop[0].data_width == round_up(cropsize, enc.w))
		assert(infop[0].data_height == round_up(cropsize, enc.h))
		assert(infop[0].block_width == enc.w)
		assert(infop[0].block_height == enc.h)

		assert(not C.jc_save_and_free(out))

		-- check gradient_tran.jpg
		add_tmp_file('checktmp.jpg')
		local out = C.jc_new('checktmp.jpg', -1, -1) assert(out ~= nil)
		assert(0 == C.jc_add_image(out, 'gradient_tran.jpg'))

		assert(C.jc_get_info(out, 0, infop))
		assert(infop[0].width == cropsize)
		assert(infop[0].height == cropsize)
		assert(infop[0].data_width == round_up(cropsize, 8))
		assert(infop[0].data_height == round_up(cropsize, 8))
		assert(infop[0].block_width == 8)
		assert(infop[0].block_height == 8)

		assert(not C.jc_save_and_free(out))
	else
		assert(check_md5_equals('gradient_tran.jpg', 'gradient_ours.jpg'))
	end
end

end

delete_tmp_files()
