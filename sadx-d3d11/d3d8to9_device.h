#pragma once

#include <array>
#include <deque>
#include <unordered_map>

#include <d3d11_1.h>
#include <wrl/client.h>

#include "Unknown.h"
#include "dirty_t.h"
#include "simple_math.h"
#include "Shader.h"
#include "cbuffers.h"
#include "hash_combine.h"
#include <future>
#include <optional>

class Direct3DBaseTexture8;
class Direct3DIndexBuffer8;
class Direct3DTexture8;
class Direct3DVertexBuffer8;
class Direct3DSurface8;

using Direct3DSwapChain8 = void;
using Direct3DCubeTexture8 = void;
using Direct3DVolumeTexture8 = void;

using Microsoft::WRL::ComPtr;

struct ShaderFlags
{
	using type = uint64_t;

	enum T : type
	{
		none,
		rs_lighting  = 0b00001000'00000000'00000000'00000000'00000000'00000000'00000000'00000000,
		rs_specular  = 0b00010000'00000000'00000000'00000000'00000000'00000000'00000000'00000000,
		rs_alpha     = 0b00100000'00000000'00000000'00000000'00000000'00000000'00000000'00000000,
		rs_fog       = 0b01000000'00000000'00000000'00000000'00000000'00000000'00000000'00000000,
		rs_oit       = 0b10000000'00000000'00000000'00000000'00000000'00000000'00000000'00000000,
		fvf_position = 0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00001110,
		fvf_fields   = 0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'11110000,
		fvf_texcount = 0b00000000'00000000'00000000'00000000'00000000'00000000'00001111'00000000,
		fvf_lastbeta = 0b00000000'00000000'00000000'00000000'00000000'00000000'00010000'00000000,
		fvf_texfmt   = 0b00000000'00000000'00000000'00000000'11111111'11111111'00000000'00000000,
		fvf_mask     = fvf_position | fvf_fields | fvf_texcount | fvf_lastbeta | fvf_texfmt,
		rs_mask      = rs_lighting | rs_specular | rs_alpha | rs_fog | rs_oit,
		mask         = rs_mask | fvf_mask,
		count
	};

	static constexpr type light_sanitize_flags = rs_lighting | rs_specular |
	                                             D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_NORMAL | D3DFVF_XYZRHW;

#ifdef PER_PIXEL
	// TODO
#else
	static constexpr type vs_mask = rs_lighting | rs_specular | fvf_mask;
	static constexpr type ps_mask = rs_mask | light_sanitize_flags;
#endif

	static type sanitize(type flags);
};

struct DepthFlags
{
	using type = uint32_t;

	enum T : type
	{
		comparison_mask = 0xF
	};
};

struct StencilFlags
{
	using type = uint32_t;

	static constexpr type op_mask     = 0xF;

	static constexpr type fail_shift  = 0;
	static constexpr type zfail_shift = 4;
	static constexpr type pass_shift  = 8;
	static constexpr type func_shift  = 12;

	static constexpr type rw_mask    = 0xFF;

	static constexpr type read_shift = 16;
	static constexpr type write_shift = 24;
};

struct DepthStencilFlags : dirty_impl
{
	using type = uint32_t;

	enum T : type
	{
		depth_test_enabled  = 1 << 0,
		depth_write_enabled = 1 << 1,
		stencil_enabled     = 1 << 2,
	};

	dirty_t<type> flags = dirty_t<type>(0);
	dirty_t<DepthFlags::type> depth_flags = dirty_t<DepthFlags::type>(0);
	dirty_t<StencilFlags::type> stencil_flags = dirty_t<StencilFlags::type>(0);

	[[nodiscard]] bool dirty() const override;
	void clear() override;
	void mark() override;

	bool operator==(const DepthStencilFlags& rhs) const;
};

namespace std
{
	template <>
	struct hash<DepthStencilFlags>
	{
		std::size_t operator()(const DepthStencilFlags& s) const noexcept
		{
			size_t h = std::hash<size_t>()(s.flags.data());

			hash_combine(h, (size_t)s.depth_flags.data());
			hash_combine(h, (size_t)s.stencil_flags.data());

			return h;
		}
	};
}

struct RasterFlags
{
	enum T : uint32_t
	{
		cull_none = 1,
		cull_front,
		cull_back,
		fill_wireframe = 2 << 2,
		fill_solid = 3 << 2
	};

	static constexpr uint32_t cull_mask = 0b0011;
	static constexpr uint32_t fill_mask = 0b1100;
};

struct SamplerSettings : dirty_impl
{
	dirty_t<D3DTEXTUREADDRESS> address_u, address_v, address_w;
	dirty_t<D3DTEXTUREFILTERTYPE> filter_mag, filter_min, filter_mip;
	dirty_t<float> mip_lod_bias;
	dirty_t<uint32_t> max_mip_level, max_anisotropy;

	SamplerSettings();

	bool operator==(const SamplerSettings& s) const;

	bool dirty() const override;
	void clear() override;
	void mark() override;
};

namespace std
{
	template <>
	struct hash<SamplerSettings>
	{
		std::size_t operator()(const SamplerSettings& s) const noexcept
		{
			size_t h = std::hash<size_t>()(s.address_u.data());

			hash_combine(h, (size_t)s.address_v.data());
			hash_combine(h, (size_t)s.address_w.data());
			hash_combine(h, (size_t)s.filter_mag.data());
			hash_combine(h, (size_t)s.filter_min.data());
			hash_combine(h, (size_t)s.filter_mip.data());
			hash_combine(h, s.mip_lod_bias.data());
			hash_combine(h, (size_t)s.max_mip_level.data());
			hash_combine(h, (size_t)s.max_anisotropy.data());

			return h;
		}
	};
}

struct StreamPair
{
	Direct3DVertexBuffer8* buffer;
	UINT stride;

	bool operator==(const StreamPair& other) const
	{
		return buffer == other.buffer &&
		       stride == other.stride;
	}
};

struct OitNode
{
	uint  draw_call;
	float depth; // fragment depth
	uint  color; // 32-bit packed fragment color
	uint  flags; // source blend, destination blend
	uint  next;  // index of the next entry, or FRAGMENT_LIST_NULL
};

class __declspec(uuid("7385E5DF-8FE8-41D5-86B6-D7B48547B6CF")) Direct3DDevice8;

class Direct3DDevice8 : public Unknown
{
	std::recursive_mutex shader_sources_mutex;
	std::unordered_map<std::string, std::vector<uint8_t>> shader_sources;
	std::vector<uint8_t> trifan_buffer;
	std::string texcount_str;
	std::string fragments_str;
	bool freeing_shaders = false;

public:
	using callback_func = std::function<void(const std::string&)>;
	std::unordered_map<std::string, std::deque<callback_func>> draw_prologues;
	std::unordered_map<std::string, std::deque<callback_func>> draw_epilogues;

	Direct3DDevice8(const Direct3DDevice8&)            = delete;
	Direct3DDevice8& operator=(const Direct3DDevice8&) = delete;

	Direct3DDevice8(Direct3D8* d3d, const D3DPRESENT_PARAMETERS8& parameters);
	~Direct3DDevice8() = default;

	HRESULT make_cbuffer(ICBuffer& interface_, ComPtr<ID3D11Buffer>& cbuffer) const;

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override;
	virtual ULONG STDMETHODCALLTYPE AddRef() override;
	virtual ULONG STDMETHODCALLTYPE Release() override;

	virtual HRESULT STDMETHODCALLTYPE TestCooperativeLevel();
	virtual UINT STDMETHODCALLTYPE GetAvailableTextureMem();
	virtual HRESULT STDMETHODCALLTYPE ResourceManagerDiscardBytes(DWORD Bytes);
	virtual HRESULT STDMETHODCALLTYPE GetDirect3D(Direct3D8** ppD3D8);
	virtual HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS8* pCaps);
	virtual HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE* pMode);
	virtual HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters);
	virtual HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT XHotSpot, UINT YHotSpot, Direct3DSurface8* pCursorBitmap);
	virtual void STDMETHODCALLTYPE SetCursorPosition(UINT XScreenSpace, UINT YScreenSpace, DWORD Flags);
	virtual BOOL STDMETHODCALLTYPE ShowCursor(BOOL bShow);
	virtual HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS8* pPresentationParameters, Direct3DSwapChain8** ppSwapChain);
	virtual HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS8* pPresentationParameters);
	void oit_composite();
	void oit_start();
	virtual HRESULT STDMETHODCALLTYPE Present(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion);
	virtual HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, Direct3DSurface8** ppBackBuffer);
	virtual HRESULT STDMETHODCALLTYPE GetRasterStatus(D3DRASTER_STATUS* pRasterStatus);
	virtual void STDMETHODCALLTYPE SetGammaRamp(DWORD Flags, const D3DGAMMARAMP* pRamp);
	virtual void STDMETHODCALLTYPE GetGammaRamp(D3DGAMMARAMP* pRamp);
	virtual HRESULT STDMETHODCALLTYPE CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, Direct3DTexture8** ppTexture);
	virtual HRESULT STDMETHODCALLTYPE CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, Direct3DVolumeTexture8** ppVolumeTexture);
	virtual HRESULT STDMETHODCALLTYPE CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, Direct3DCubeTexture8** ppCubeTexture);
	virtual HRESULT STDMETHODCALLTYPE CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, Direct3DVertexBuffer8** ppVertexBuffer);
	virtual HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, Direct3DIndexBuffer8** ppIndexBuffer);
	virtual HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, BOOL Lockable, Direct3DSurface8** ppSurface);
	virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, Direct3DSurface8** ppSurface);
	virtual HRESULT STDMETHODCALLTYPE CreateImageSurface(UINT Width, UINT Height, D3DFORMAT Format, Direct3DSurface8** ppSurface);
	virtual HRESULT STDMETHODCALLTYPE CopyRects(Direct3DSurface8* pSourceSurface, const RECT* pSourceRectsArray, UINT cRects, Direct3DSurface8* pDestinationSurface, const POINT* pDestPointsArray);
	virtual HRESULT STDMETHODCALLTYPE UpdateTexture(Direct3DBaseTexture8* pSourceTexture, Direct3DBaseTexture8* pDestinationTexture);
	virtual HRESULT STDMETHODCALLTYPE GetFrontBuffer(Direct3DSurface8* pDestSurface);
	virtual HRESULT STDMETHODCALLTYPE SetRenderTarget(Direct3DSurface8* pRenderTarget, Direct3DSurface8* pNewZStencil);
	virtual HRESULT STDMETHODCALLTYPE GetRenderTarget(Direct3DSurface8** ppRenderTarget);
	virtual HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(Direct3DSurface8** ppZStencilSurface);
	virtual HRESULT STDMETHODCALLTYPE BeginScene();
	virtual HRESULT STDMETHODCALLTYPE EndScene();
	virtual HRESULT STDMETHODCALLTYPE Clear(DWORD Count, const D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil);
	virtual HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE State, const matrix* pMatrix);
	virtual HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE State, matrix* pMatrix);
	virtual HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE State, const matrix* pMatrix);
	virtual HRESULT STDMETHODCALLTYPE SetViewport(const D3DVIEWPORT8* pViewport);
	virtual HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT8* pViewport);
	virtual HRESULT STDMETHODCALLTYPE SetMaterial(const D3DMATERIAL8* pMaterial);
	virtual HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL8* pMaterial);
	virtual HRESULT STDMETHODCALLTYPE SetLight(DWORD Index, const D3DLIGHT8* pLight);
	virtual HRESULT STDMETHODCALLTYPE GetLight(DWORD Index, D3DLIGHT8* pLight);
	virtual HRESULT STDMETHODCALLTYPE LightEnable(DWORD Index, BOOL Enable);
	virtual HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD Index, BOOL* pEnable);
	virtual HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD Index, const float* pPlane);
	virtual HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD Index, float* pPlane);
	virtual HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE State, DWORD Value);
	virtual HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue);
	virtual HRESULT STDMETHODCALLTYPE BeginStateBlock();
	virtual HRESULT STDMETHODCALLTYPE EndStateBlock(DWORD* pToken);
	virtual HRESULT STDMETHODCALLTYPE ApplyStateBlock(DWORD Token);
	virtual HRESULT STDMETHODCALLTYPE CaptureStateBlock(DWORD Token);
	virtual HRESULT STDMETHODCALLTYPE DeleteStateBlock(DWORD Token);
	virtual HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE Type, DWORD* pToken);
	virtual HRESULT STDMETHODCALLTYPE SetClipStatus(const D3DCLIPSTATUS8* pClipStatus);
	virtual HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS8* pClipStatus);
	virtual HRESULT STDMETHODCALLTYPE GetTexture(DWORD Stage, Direct3DBaseTexture8** ppTexture);
	virtual HRESULT STDMETHODCALLTYPE SetTexture(DWORD Stage, Direct3DBaseTexture8* pTexture);
	virtual HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue);
	virtual HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value);
	virtual HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD* pNumPasses);
	virtual HRESULT STDMETHODCALLTYPE GetInfo(DWORD DevInfoID, void* pDevInfoStruct, DWORD DevInfoStructSize);
	virtual HRESULT STDMETHODCALLTYPE SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY* pEntries);
	virtual HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries);
	virtual HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(UINT PaletteNumber);
	virtual HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(UINT* pPaletteNumber);

	void run_draw_prologues(const std::string& callback);
	void run_draw_epilogues(const std::string& callback);

	virtual HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount);
	virtual HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT MinIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount);
	virtual HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride);
	virtual HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertexIndices, UINT PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride);
	virtual HRESULT STDMETHODCALLTYPE ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, Direct3DVertexBuffer8* pDestBuffer, DWORD Flags);
	virtual HRESULT STDMETHODCALLTYPE CreateVertexShader(const DWORD* pDeclaration, const DWORD* pFunction, DWORD* pHandle, DWORD Usage);
	virtual HRESULT STDMETHODCALLTYPE SetVertexShader(DWORD Handle);
	virtual HRESULT STDMETHODCALLTYPE GetVertexShader(DWORD* pHandle);
	virtual HRESULT STDMETHODCALLTYPE DeleteVertexShader(DWORD Handle);
	virtual HRESULT STDMETHODCALLTYPE SetVertexShaderConstant(DWORD Register, const void* pConstantData, DWORD ConstantCount);
	virtual HRESULT STDMETHODCALLTYPE GetVertexShaderConstant(DWORD Register, void* pConstantData, DWORD ConstantCount);
	virtual HRESULT STDMETHODCALLTYPE GetVertexShaderDeclaration(DWORD Handle, void* pData, DWORD* pSizeOfData);
	virtual HRESULT STDMETHODCALLTYPE GetVertexShaderFunction(DWORD Handle, void* pData, DWORD* pSizeOfData);
	virtual HRESULT STDMETHODCALLTYPE SetStreamSource(UINT StreamNumber, Direct3DVertexBuffer8* pStreamData, UINT Stride);
	virtual HRESULT STDMETHODCALLTYPE GetStreamSource(UINT StreamNumber, Direct3DVertexBuffer8** ppStreamData, UINT* pStride);
	virtual HRESULT STDMETHODCALLTYPE SetIndices(Direct3DIndexBuffer8* pIndexData, UINT BaseVertexIndex);
	virtual HRESULT STDMETHODCALLTYPE GetIndices(Direct3DIndexBuffer8** ppIndexData, UINT* pBaseVertexIndex);
	virtual HRESULT STDMETHODCALLTYPE CreatePixelShader(const DWORD* pFunction, DWORD* pHandle);
	virtual HRESULT STDMETHODCALLTYPE SetPixelShader(DWORD Handle);
	virtual HRESULT STDMETHODCALLTYPE GetPixelShader(DWORD* pHandle);
	virtual HRESULT STDMETHODCALLTYPE DeletePixelShader(DWORD Handle);
	virtual HRESULT STDMETHODCALLTYPE SetPixelShaderConstant(DWORD Register, const void* pConstantData, DWORD ConstantCount);
	virtual HRESULT STDMETHODCALLTYPE GetPixelShaderConstant(DWORD Register, void* pConstantData, DWORD ConstantCount);
	virtual HRESULT STDMETHODCALLTYPE GetPixelShaderFunction(DWORD Handle, void* pData, DWORD* pSizeOfData);
	virtual HRESULT STDMETHODCALLTYPE DrawRectPatch(UINT Handle, const float* pNumSegs, const D3DRECTPATCH_INFO* pRectPatchInfo);
	virtual HRESULT STDMETHODCALLTYPE DrawTriPatch(UINT Handle, const float* pNumSegs, const D3DTRIPATCH_INFO* pTriPatchInfo);
	virtual HRESULT STDMETHODCALLTYPE DeletePatch(UINT Handle);

	void print_info_queue() const;

	std::vector<D3D_SHADER_MACRO> shader_preprocess(ShaderFlags::type flags);
	const std::vector<uint8_t>& get_shader_source(const std::string& path);
	VertexShader get_vs(ShaderFlags::type flags, bool speedy_speed_boy, std::unordered_map<ShaderFlags::type, VertexShader>& shaders, std::recursive_mutex& mutex);
	PixelShader get_ps(ShaderFlags::type flags, bool speedy_speed_boy, std::unordered_map<ShaderFlags::type, PixelShader>& shaders, std::recursive_mutex& mutex);
	void create_depth_stencil();
	void create_render_target();
	void create_native();
	bool set_primitive_type(D3DPRIMITIVETYPE primitive_type) const;
	static bool primitive_vertex_count(D3DPRIMITIVETYPE primitive_type, uint32_t& count);
	void oit_zwrite_force(DWORD& ZWRITEENABLE, DWORD& ZENABLE);
	void oit_zwrite_restore(DWORD ZWRITEENABLE, DWORD ZENABLE);
	bool update_input_layout();
	void commit_per_pixel();
	void commit_per_model();
	void commit_per_scene();
	void commit_per_texture();
	void update_sampler();
	std::optional<PixelShader> get_ps_async(ShaderFlags::type flags);
	void compile_shaders(ShaderFlags::type flags, VertexShader& vs, PixelShader& ps);
	void update_shaders();
	void update_blend();
	void update_depth();
	void update_rasterizers();
	bool update();
	bool skip_draw() const;
	void free_shaders();
	void oit_load_shaders();
	void oit_release();
	void update_wv_inv_t();

	ShaderFlags::type shader_flags      = ShaderFlags::none;
	ShaderFlags::type last_shader_flags = ShaderFlags::mask;

	VertexShader current_vs;
	PixelShader current_ps;

	std::recursive_mutex ps_mutex, ps_speed_mutex, vs_mutex, vs_speed_mutex;

	std::unordered_map<ShaderFlags::type, VertexShader> vertex_shaders;
	std::unordered_map<ShaderFlags::type, PixelShader> pixel_shaders;

	std::recursive_mutex vs_tasks_mutex, ps_tasks_mutex;
	std::unordered_map<ShaderFlags::type, std::future<VertexShader>> vs_tasks;
	std::unordered_map<ShaderFlags::type, std::future<PixelShader>> ps_tasks;

	std::unordered_map<ShaderFlags::type, VertexShader> vertex_speed_shaders;
	std::unordered_map<ShaderFlags::type, PixelShader>  pixel_speed_shaders;

	D3DPRESENT_PARAMETERS8 present_params {};
	ComPtr<IDXGISwapChain> swap_chain;

	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	ComPtr<ID3D11InfoQueue> info_queue;

	bool oit_enabled = true;

protected:
	bool oit_actually_enabled = false;
	Direct3D8* const d3d;

	VertexShader composite_vs;
	PixelShader composite_ps;

	ComPtr<ID3D11RenderTargetView> back_buffer_view;

	ComPtr<ID3D11Texture2D> composite_texture;
	ComPtr<ID3D11RenderTargetView> composite_view;
	ComPtr<ID3D11ShaderResourceView> composite_srv;

	ComPtr<ID3D11Texture2D>           FragListHead;
	ComPtr<ID3D11ShaderResourceView>  FragListHeadSRV;
	ComPtr<ID3D11UnorderedAccessView> FragListHeadUAV;

	ComPtr<ID3D11Texture2D>           FragListCount;
	ComPtr<ID3D11ShaderResourceView>  FragListCountSRV;
	ComPtr<ID3D11UnorderedAccessView> FragListCountUAV;

	ComPtr<ID3D11Buffer>              FragListNodes;
	ComPtr<ID3D11ShaderResourceView>  FragListNodesSRV;
	ComPtr<ID3D11UnorderedAccessView> FragListNodesUAV;

	std::unordered_map<DWORD, Direct3DTexture8*> textures;
	std::unordered_map<DWORD, SamplerSettings> sampler_setting_values;
	std::array<dirty_t<DWORD>, 174> render_state_values;
	std::unordered_map<ShaderFlags::type, ComPtr<ID3D11InputLayout>> fvf_layouts;
	std::unordered_map<DWORD, StreamPair> stream_sources;

	dirty_t<uint32_t> raster_flags;
	std::unordered_map<uint32_t, ComPtr<ID3D11RasterizerState>> raster_states;

	std::unordered_map<SamplerSettings, ComPtr<ID3D11SamplerState>> sampler_states;

	dirty_t<uint32_t> blend_flags;
	std::unordered_map<uint32_t, ComPtr<ID3D11BlendState>> blend_states;

	ComPtr<Direct3DIndexBuffer8> index_buffer = nullptr;

	void oit_write();
	void oit_read() const;
	void oit_init();
	void FragListHead_Init();
	void FragListCount_Init();
	void FragListNodes_Init();
	void up_get(size_t target_size);

	ComPtr<Direct3DVertexBuffer8> up_buffer;
	std::deque<ComPtr<Direct3DVertexBuffer8>> up_buffers;

	dirty_t<DWORD> FVF;
	ComPtr<Direct3DTexture8> depth_stencil;
	ComPtr<Direct3DTexture8> back_buffer;
	ComPtr<Direct3DTexture8> composite_wrapper;

	DepthStencilFlags depthstencil_flags {};
	std::unordered_map<DepthStencilFlags, ComPtr<ID3D11DepthStencilState>> depth_states;

	ComPtr<Direct3DSurface8> current_render_target;
	ComPtr<Direct3DSurface8> current_depth_stencil;

	ComPtr<ID3D11Buffer> per_scene_cbuf;
	ComPtr<ID3D11Buffer> per_model_cbuf;
	ComPtr<ID3D11Buffer> per_pixel_cbuf;
	ComPtr<ID3D11Buffer> per_texture_cbuf;
	PerSceneBuffer per_scene {};
	PerModelBuffer per_model {};
	PerPixelBuffer per_pixel {};
	TextureStages per_texture {};

	INT current_base_vertex_index = 0;
	//const BOOL ZBufferDiscarding = FALSE;
	DWORD current_vertex_shader_handle = 0;
	//DWORD CurrentPixelShaderHandle = 0;
	bool palette_flag = false;

	static constexpr size_t MAX_CLIP_PLANES    = 6;
	float stored_clip_planes[MAX_CLIP_PLANES][4] = {};
	//DWORD ClipPlaneRenderState = 0;

	D3D11_VIEWPORT viewport {};
	D3DMATERIAL8 material {};
};
