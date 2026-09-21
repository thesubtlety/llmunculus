
Local AI Model Files: Formats, Storage Economics, and Passive Inspection
Executive summary
A “local AI model format” is not one thing. Some formats are tensor containers such as Safetensors and legacy PyTorch checkpoints; some are graph-plus-tensor interchange formats such as ONNX and TensorFlow Lite/LiteRT; some are deployment packages such as TensorFlow SavedModel, Core ML packages, and MLC artifacts; and some are inference-native containers such as GGUF. Hugging Face Transformers itself is primarily a directory/repository convention that combines configuration, tokenizer assets, and one or more weight files rather than defining a new binary tensor format. 

The most useful distinction for forensic or passive inspection is whether the representation is self-describing. Safetensors exposes tensor names, shapes, dtypes, byte ranges, and optional metadata in a JSON header; ONNX exposes graph topology, initializers, operators, producer metadata, and optional external tensor locations; GGUF incorporates architecture, tokenizer, quantization information, tensor descriptors, and weights into one file. By contrast, a bare PyTorch state_dict may reveal tensor names and shapes but depends heavily on external Python architecture code for semantics. 

For an ordinary dense model, the first-order storage equation is:

[ \text{weight bytes}\approx N_{\text{parameters}}\times\frac{\text{bits per parameter}}{8}. ]

Thus a 7-billion-parameter dense model is about 28 GB at FP32, 14 GB at FP16/BF16, 7 GB at 8 bits, 3.5 GB at an ideal four bits, and 1.75 GB at an ideal two bits. Actual quantized files are larger than the ideal because they also store scales, zero-points/minima, block metadata, alignment, and sometimes unquantized embeddings, normalization weights, or output layers. Apple gives the same underlying arithmetic in an official example: 25 million FP16 parameters occupy about 50 MB before compression. 

Passive inspection can often establish the declared architecture, dimensions, parameter count, tensor dtypes, tokenizer/vocabulary, quantization representation, graph operators, model/version metadata, and—in training checkpoints—optimizer state if it was actually saved. It generally cannot prove the training dataset, exact optimization history, number of training tokens, factual provenance, whether a model was distilled, whether an apparent ablation was intentional, or whether two differently serialized models have identical behavior unless those facts are recorded or independently corroborated. A file timestamp is not provenance, and a checksum proves byte identity, not authorship.

Quantization, pruning, weight clustering/sharing, and suspicious alterations leave measurable signals. Good tests include exact-zero sparsity, row/column/block sparsity, unique-value counts, quantization-grid residuals, byte/symbol entropy, histograms, layer-wise L1/L2/Frobenius norms, singular-value/rank profiles, per-tensor hashes, and comparison against an expected architecture or trusted checkpoint. None is a universal proof: for example, high byte entropy can arise from ordinary floating-point tensors as well as encryption, and many quantizers deliberately leave selected tensors at higher precision.

The safest workflow is to hash first, inspect containers and textual metadata without executing anything, use format-aware parsers, inventory tensors and graph structure, then perform numerical statistics one tensor at a time. This matters especially with traditional PyTorch files because torch.save/torch.load historically uses Python pickle; PyTorch recommends state dictionaries for model weights, and Hugging Face prefers Safetensors when available in part because pickle-based loading has security implications. 

Local model artifact

Representation family

Tensor container

Graph + tensors

Repository / package

Inference-native container

PyTorch .pt/.pth

Safetensors

ONNX

TFLite / LiteRT

Core ML .mlmodel

Hugging Face directory

TensorFlow SavedModel

Core ML .mlpackage

MLC artifact directory

GGUF

Legacy GGML / GGMF / GGJT



Show code
Format inventory and on-disk anatomy
The table below treats “typical” as a mainstream current representation rather than a guarantee. File extensions alone are weak identifiers; .pt and .pth, in particular, are conventions and can contain materially different PyTorch objects. PyTorch explicitly describes those extensions as conventions rather than a formal requirement. 

Format / ecosystem	Typical file or directory structure	Common inspectable metadata	Parameter representation	Layout / dtype / endianness
PyTorch .pt, .pth, .bin	Modern torch.save: ZIP64 containing data.pkl, byteorder, data/0, data/1…, version; older serialization variants also exist	Python/state-dict keys, tensor sizes/strides/storage references; arbitrary saved dictionaries can contain epoch, optimizer, scheduler, etc.	Tensor metadata in pickle; underlying storages saved separately	Tensor dtype/shape/stride come from serialized objects; byteorder records little or big; storage aliasing/views can be preserved 
Hugging Face Transformers repository	Usually config.json; model.safetensors or shards plus model.safetensors.index.json; tokenizer files such as tokenizer.json, tokenizer_config.json, model-specific vocab/merges/SentencePiece assets; optional generation/chat/processor files	Architecture configuration, model type, vocab size, layer dimensions, dtype hints, tokenizer class/special tokens/chat templates; shard index maps tensor → file	Usually Safetensors today; older repos commonly use pytorch_model.bin/pickle	Determined by underlying weight format. HF prefers Safetensors when available 
Safetensors .safetensors	One file: 8-byte header length → JSON header → contiguous tensor-data region	Tensor name, dtype, shape, data_offsets; optional string-to-string __metadata__	Raw packed tensor byte ranges without executable pickle	Unsigned little-endian header length; tensor data defined little-endian; C/row-major contiguous representation; no arbitrary strides 
ONNX .onnx	Usually one protobuf ModelProto; large tensors may be external files	IR/opset versions, producer name/version, domain, model version, doc strings, graph nodes, tensor names/shapes, metadata properties, functions, training info	TensorProto inline typed fields or raw_data; alternatively external-data blobs	raw_data fixed-width values are little-endian; INT4/UINT4 pack two values per byte, INT2/UINT2 four per byte; external descriptors can include location, offset, length, checksum 
TensorFlow SavedModel	Directory with saved_model.pb, variables/, optionally assets/, assets.extra/, and fingerprint.pb	Signatures, computation graph/functions, tensor specs, variable names, assets; fingerprint hashes	Variable values are stored in TensorFlow checkpoint files in variables/; constants may also appear in protobuf graph data	Do not infer raw checkpoint byte layout manually; use TensorFlow checkpoint readers. saved_model.pb is protobuf. 
TensorFlow GraphDef .pb	Single serialized protobuf, often used for graphs/frozen graphs	Nodes, op types, attributes, shapes where recorded, constants	Frozen weights can be represented as Const tensors inside graph; non-frozen GraphDef may not contain variables	Protobuf/TensorProto representation; unlike SavedModel, a lone .pb is not necessarily a complete reusable TF program
TFLite / LiteRT .tflite	FlatBuffer, normally a single model file; metadata and associated files can be appended/bundled	Subgraphs, tensors, operators, buffers, input/output shapes/types, quantization scales/zero-points; optional model metadata, license/description, labels/assets	Constant tensors referenced through FlatBuffer Buffer objects; integer/float quantized forms supported	FlatBuffers use little-endian binary representation; quantized tensor scale and zero-point are explicitly accessible in model metadata/APIs 
Legacy GGML / GGMF / GGJT	Generally single inference files from earlier ggml/llama.cpp generations	Version-dependent; tensor descriptors and limited model metadata	Raw or block-quantized ggml tensors	Historical variants are not one stable interchange specification; prefer version-specific parser
GGUF / current llama.cpp	Single file: header → metadata KV pairs → tensor descriptors → aligned tensor data	Architecture, name, author, version, quantization version, license/repository information, context parameters, tokenizer model/tokens/merges/special IDs/chat template, tensor names/shapes/types	F32/F16/BF16 plus extensive block-quantized GGML types: Q8, Q6, Q5, Q4, Q3, Q2, IQ/TQ/MXFP families, etc.	Designed for aligned/mmap-friendly tensor storage; little-endian is normal; GGUF v3 can be converted for opposite endian. Tensor descriptors contain dimensions/type/offset. 
MLC LLM artifact bundle	Common converted-weight directory: mlc-chat-config.json, tensor-cache.json, params_shard_0.bin…, tokenizer files; compiled model library produced separately	Architecture/configuration, quantization scheme, context/sliding-window/prefill settings, tokenizer configuration, weight-cache index	Runtime-specific parameter shards described by the tensor cache; examples include q0f16 and grouped four-bit schemes such as q4f16_*	MLC is an artifact pipeline, not one universal tensor wire format. Interpret shard layout through matching MLC/TVM version rather than guessing raw bytes. 
Core ML .mlmodel	Single protobuf Core ML model	Model description/interface, inputs/outputs, model metadata, architecture/operators, weights depending on model type	Neural-network weights can reside in protobuf representation	Portable Core ML exchange artifact; exact tensor representation depends on specification/model type 
Core ML .mlpackage	Package/directory separating model specification, metadata, and large weights/resources	Manifest/package information plus Core ML model metadata and interface	ML Program weights are stored separately from architecture, enabling very large external weight blobs	Current preferred packaging for ML Program-style models; compressed representations can include pruning, linear quantization, and palettization 

PyTorch checkpoints
A modern default torch.save() checkpoint is especially interesting because it is both a logical object serialization and a simple ZIP container. PyTorch documents the typical archive as:

text
Copy
checkpoint.pth
├── data.pkl
├── byteorder
├── data/
│   ├── 0
│   ├── 1
│   ├── 2
│   └── ...
└── version
data.pkl contains the pickled object graph with tensor storage separated out; individual storages live under data/. The format preserves relationships between tensors and storage, including views and sharing. The byteorder member, added in PyTorch 2.1, records the originating system byte order, and ZIP members are aligned in a way intended to support efficient checkpoint manipulation. 

A common inference checkpoint is simply:

python
Copy
{
    "model.embed_tokens.weight": Tensor(...),
    "model.layers.0.self_attn.q_proj.weight": Tensor(...),
    ...
}
but a training checkpoint can instead be a dictionary such as:

text
Copy
{
  model_state_dict: ...,
  optimizer_state_dict: ...,
  epoch: ...,
  loss: ...,
  scheduler: ...
}
There is no requirement that such fields exist; torch.save can serialize arbitrary pickle-compatible Python structures. Consequently, filenames like model.pt do not prove that a file contains only weights. PyTorch recommends saving/loading a module’s state_dict rather than serializing the entire Python module when portability is desired. 

Hugging Face and Safetensors
A contemporary Transformers repository often resembles:

text
Copy
my-model/
├── config.json
├── generation_config.json                 # optional/common
├── model.safetensors
│
├── tokenizer.json
├── tokenizer_config.json
├── special_tokens_map.json                # model/version dependent
├── vocab.json / vocab.txt / tokenizer.model
├── merges.txt                             # e.g. BPE, if applicable
├── chat_template.jinja                    # newer chat-template layout
└── README.md / LICENSE / other sidecars
Large models may instead have:

text
Copy
model-00001-of-00008.safetensors
model-00002-of-00008.safetensors
...
model-00008-of-00008.safetensors
model.safetensors.index.json
The shard index has a weight_map mapping parameter names to shard files and metadata such as aggregate size. Transformers can infer or record dtype in configuration; tokenizer loading consults tokenizer_config.json, and current Hugging Face tokenizers can serialize the tokenization pipeline in unified tokenizer.json form while retaining model-specific vocabulary formats when required. Newer Transformers saves standalone Jinja chat templates, while older repositories may embed them in tokenizer_config.json. 

Safetensors itself is unusually easy to inspect without importing ML code:

text
Copy
offset 0
┌─────────────────────────────┐
│ uint64 LE: JSON length N    │  8 bytes
├─────────────────────────────┤
│ UTF-8 JSON header           │  N bytes
│  tensor_name:               │
│    dtype                    │
│    shape                    │
│    data_offsets [a,b]       │
│  __metadata__: {...}        │
├─────────────────────────────┤
│ tensor bytes                │
│ tensor bytes                │
│ ...                         │
└─────────────────────────────┘
The specification defines little-endian tensors, C/row-major layout, and contiguous byte ranges; arbitrary strided views are not directly represented. 

ONNX
ONNX is closer to a self-contained intermediate representation than a checkpoint. The top-level ModelProto can identify IR and opset versions, producer, model version, domain and documentation and contains a graph whose nodes are topologically represented along with graph inputs, outputs, value information, and initializer tensors. Optional training information and model-defined metadata properties are also part of the schema. 

Large initializers may be external:

text
Copy
model.onnx
weights.bin
An external tensor descriptor can carry a relative location, optional byte offset and length, and an optional checksum. ONNX explicitly specifies little-endian raw tensor data and even standardizes nibble/two-bit packing: INT4/UINT4 place the first value in the low nibble and the second in the high nibble; INT2/UINT2 place four values successively from least-significant to most-significant bits. 

TensorFlow and LiteRT
SavedModel is a directory-level representation of a complete TensorFlow program. saved_model.pb carries the program/signatures, variables/ contains checkpoint data, and assets/ can contain vocabularies or other graph dependencies. Modern SavedModels can contain fingerprint.pb, holding hashes intended to identify parts of the SavedModel. Some Python-only attributes, functions, and arbitrary data are not automatically preserved merely because an object was saved. 

TensorFlow Lite, renamed/continued in Google’s LiteRT ecosystem, is fundamentally a FlatBuffer representation. It can also contain an optional metadata FlatBuffer named TFLITE_METADATA. Google’s metadata specification supports model descriptions, authors/license information, input/output semantics, normalization data and associated files; associated vocabularies or labels can be bundled by appending a ZIP section while retaining the .tflite extension. Quantization scales and zero-points are stored in the TFLite model and are retrievable through tensor APIs. 

GGML, GGUF, and llama.cpp
The useful modern answer to “llama.cpp format” is GGUF. The ggml project describes GGUF as the successor to its older GGML, GGMF, and GGJT file formats and as a single-file, extensible, mmap-friendly representation containing key-value metadata and tensors. 

Conceptually:

text
Copy
model.gguf
┌──────────────────────────────────────┐
│ magic / version                     │
│ tensor_count                        │
│ metadata_kv_count                   │
├──────────────────────────────────────┤
│ metadata KV records                 │
│  general.architecture               │
│  general.name                       │
│  general.quantization_version       │
│  general.license / repo / author    │
│  tokenizer.ggml.*                   │
│  architecture-specific properties   │
├──────────────────────────────────────┤
│ tensor descriptors                  │
│  name                               │
│  dimensions                         │
│  GGML data/quantization type        │
│  offset                             │
├──────────────────────────────────────┤
│ alignment padding                   │
├──────────────────────────────────────┤
│ tensor data                         │
└──────────────────────────────────────┘
GGUF defines standardized metadata for architecture and quantization as well as provenance-like fields including author, organization, license, URL/repository information, quantizer, description and version. Its tokenizer namespace can carry tokens, scores, token types, merges, special-token IDs, a Hugging Face tokenizer JSON, and chat templates. Those fields are powerful evidence about intended use—but because ordinary metadata can be rewritten, they are claims inside the artifact rather than cryptographically trustworthy provenance unless externally authenticated. 

The official llama.cpp gguf-py package includes a reader example, metadata dumper, endian converter, metadata editor and GUI editor. 

MLC and Core ML
MLC is best understood as a deployment compilation pipeline. Converted model weights commonly appear as params_shard_*.bin files indexed by tensor-cache.json, beside mlc-chat-config.json and tokenizer data. The compiled model code/library is a separate artifact and may be a platform-specific .so, .dylib, .dll, .wasm, or other target output. Consequently, a .bin shard by itself is not usefully self-describing; its index and the matching MLC/TVM runtime version matter. 

Core ML similarly has two useful levels. Apple’s older/simpler .mlmodel is a protobuf-based model representation. .mlpackage separates architecture and large weights/resources; ML Program models rely on the package representation specifically because their weights are separated from the architecture. Apple’s official compression tooling supports sparse/pruned representations, quantized weights and palettization/weight clustering, so a small Core ML file should not automatically be interpreted as having proportionally fewer semantic parameters. 

Storage economics and size mappings
Raw dense size
The most portable sizing rule is independent of format:

[ S_{\mathrm{GB}} = \frac{P_{\mathrm{billions}}\times b}{8} ]

where (P_{\mathrm{billions}}) is the number of parameters in billions, (b) is bits per stored parameter, and GB means decimal (10^9) bytes.

These are ideal dense payload sizes only.

Parameter count	FP32, 32-bit	FP16/BF16, 16-bit	INT8, 8-bit	Ideal 4-bit	Ideal 2-bit
1B	4 GB	2 GB	1 GB	0.50 GB	0.25 GB
3B	12 GB	6 GB	3 GB	1.50 GB	0.75 GB
7B	28 GB	14 GB	7 GB	3.50 GB	1.75 GB
8B	32 GB	16 GB	8 GB	4 GB	2 GB
13B	52 GB	26 GB	13 GB	6.50 GB	3.25 GB
34B	136 GB	68 GB	34 GB	17 GB	8.50 GB
70B	280 GB	140 GB	70 GB	35 GB	17.50 GB
109B	436 GB	218 GB	109 GB	54.50 GB	27.25 GB
405B	1.62 TB	810 GB	405 GB	202.5 GB	101.25 GB

To convert these decimal-GB figures to GiB, multiply by approximately 0.9313. For example, 14 GB is about 13.0 GiB.

These values represent parameters, not runtime memory. Loading a model can additionally require runtime bookkeeping, temporary dequantization buffers, activations, KV cache, allocator overhead, and—during training—gradients and optimizer state.

Why “four-bit” rarely means exactly four bits per parameter
Block quantization normally needs auxiliary values. Suppose a group of (G) weights carries a (q)-bit code per weight plus one FP16 scale:

[ \text{effective bpw}=q+\frac{16}{G}. ]

With both an FP16 scale and FP16 zero-point/minimum:

[ \text{effective bpw}=q+\frac{32}{G}. ]

Therefore:

Nominal representation	Illustrative group metadata	Effective payload before other overhead
INT8, group 64	one FP16 scale	8.25 bits/weight
INT8, group 32	one FP16 scale	8.50 bits/weight
4-bit, group 64	one FP16 scale	4.25 bits/weight
4-bit, group 32	one FP16 scale	4.50 bits/weight
4-bit, group 64	scale + zero/min	4.50 bits/weight
4-bit, group 32	scale + zero/min	5.00 bits/weight
2-bit, group 64	one FP16 scale	2.25 bits/weight
2-bit, group 32	one FP16 scale	2.50 bits/weight
2-bit, group 32	scale + zero/min	3.00 bits/weight

This arithmetic is illustrative, not a promise about a particular quantizer. GGUF, GPTQ-style, AWQ-style, Core ML palettization and other schemes use different blocks, codebooks, scales, minima and exceptions. GGUF in particular has many data types with scheme-specific packing rather than one generic “Q4” representation. 

A useful field estimate for complete inference files is therefore:

Class	First-order bits on disk per original parameter
Dense FP32	~32
Dense FP16/BF16	~16
Dense INT8	~8, plus small headers/metadata
Scaled/groupwise INT8	commonly slightly above 8
Nominal four-bit group quantization	often roughly 4.2–5+
Nominal two-bit group/codebook quantization	often roughly 2.2–3+

The qualifier “per original parameter” matters for mixture-of-experts models, low-rank adapters, weight sharing, sparse encodings and compressed deployment graphs. Total parameter count can differ from active parameter count, and a sparse/clustered representation can encode the logical dense tensor without writing every scalar separately.

Concrete examples
A nominal 7B transformer in FP16 has a theoretical 14 GB weight payload. A simple four-bit representation with 4.5 effective bits/weight would instead require:

[ 7\times 4.5/8 \approx 3.94\text{ GB}. ]

A 70B model at the same effective rate would require about:

[ 70\times4.5/8 \approx39.4\text{ GB}. ]

At 2.5 bits/weight, a 70B payload is about 21.9 GB, not the ideal 17.5 GB.

Apple’s Core ML compression explanation illustrates the same principles in another ecosystem: it describes FP16 as two bytes per weight, INT8 quantization as approximately halving that weight representation, and palettization as storing indices into a centroid/codebook table; a two-bit palette can represent four centroids per index. 

Pruning is more subtle. A dense serialization that merely sets weights to zero may save no uncompressed disk space at all; it becomes smaller only under compression or when the target format encodes sparse values and indices. Google’s LiteRT optimization documentation specifically notes that pruning can leave the uncompressed model size unchanged while increasing compressibility, whereas Apple’s Core ML compressed sparse representation can avoid storing all zero elements explicitly. The difference illustrates why “50% sparsity” does not imply “50% of the file size” independent of representation. 

What passive inspection can and cannot reveal
High-confidence observations
With a properly parsed artifact, the following are often directly observable rather than inferred:

Property	Reliability when present	Examples
Tensor names	High	layers.7.self_attn.q_proj.weight
Tensor shape	High	[4096, 4096]
Stored dtype / quant type	High	F16, BF16, INT8, GGML Q4_K-style type
Stored parameter count	High	Sum of tensor element counts, with caveats for packed/sparse representations
Graph operators	High in graph formats	ONNX MatMul, Conv; TFLite operator codes
Declared architecture	High as a statement, not provenance	HF model_type; GGUF general.architecture
Tokenizer/vocab	High when embedded/present	HF tokenizer assets; GGUF tokenizer metadata
Input/output signatures	High in graph/package formats	ONNX graph I/O; SavedModel signatures
Quantization parameters	High if explicit	scales, zero-points, GGUF tensor type
External data locations	High	ONNX external-data records
Checksums/fingerprints	High as recorded values	ONNX external checksum; TF fingerprint.pb
License/author/repository strings	High as metadata claims	GGUF general metadata; TFLite/Core ML metadata

ONNX has unusually rich standardized top-level metadata, while GGUF explicitly standardizes many model and tokenizer fields. TFLite metadata can contain author/license and semantic descriptions. TensorFlow SavedModel can expose signatures and fingerprints. 

Architecture reconstruction
Architecture is easiest when both a graph and weight names exist. ONNX and TFLite explicitly describe operators and tensor connections. SavedModel contains executable graph/functions and signatures. A Hugging Face repository often specifies architecture hyperparameters directly in config.json; Transformers configuration can expose values such as vocabulary size, hidden dimensions, number of layers and activation choices. GGUF has standardized architecture-specific metadata plus recognizable standardized tensor naming conventions. 

Even a bare state dictionary can reveal a great deal by dimensional inference. For a transformer, repeated groups of tensors can suggest:

text
Copy
number of blocks
hidden dimension
feed-forward/intermediate dimension
number or dimension of attention projections
vocabulary size
whether embedding and output dimensions match
presence of normalization biases
presence of adapters / LoRA matrices
But tensor shapes do not always uniquely determine the computation. Fused QKV projections, grouped-query attention, transposed storage conventions, reshaped convolutions, custom kernels and tensor-parallel sharding can make multiple architectures consistent with the same dimensions.

Tokenizers and vocabulary
Tokenizers are often among the most recoverable semantic artifacts. Hugging Face’s unified tokenizer.json can describe a complete tokenization pipeline; tokenizer_config.json specifies tokenizer class/configuration, while some models use vocabulary files, merge tables or SentencePiece assets. GGUF can embed token strings, scores, token types, merges, special-token IDs and chat templates directly in the model. 

From these files you can usually answer:

vocabulary size and literal vocabulary entries;
special-token IDs;
BPE merge information or other tokenizer model data;
normalization/pre-tokenization configuration when serialized;
default chat formatting if a chat template is present.
What you cannot conclude from a vocabulary is that every token appeared in training, how frequently each token occurred, or what corpus generated it.

Training hyperparameters and optimizer state
A common analytical mistake is to treat architecture configuration as training configuration. Fields such as hidden_size, num_hidden_layers, rope_theta, vocab_size, and context limits tell you about the model definition. They normally do not tell you the learning rate schedule, training batch size, number of tokens, optimizer choice, warmup, weight decay, gradient clipping or curriculum unless those values were separately saved.

Training checkpoints can be different. A PyTorch checkpoint is capable of carrying an optimizer state_dict, scheduler state, epoch/step counts, RNG state or arbitrary user dictionaries because torch.save can serialize arbitrary supported Python objects. If Adam moment tensors are present, they can significantly increase checkpoint size. But the absence of optimizer tensors proves only that they are absent from this artifact—not that a particular optimizer was never used. 

Provenance, timestamps, and checksums
Treat provenance evidence in tiers:

Strongest: a cryptographic hash independently published by a trusted issuer, signed release metadata, a verifiable repository commit/object, or reproducible binary identity.

Moderate: internal metadata naming a source model/repository, conversion software/version, quantizer and author.

Weak: filenames, filesystem creation/modification timestamps, ZIP timestamps, directory names or free-form author strings.

A SHA-256 digest can answer “are these bytes identical?” with extremely high confidence, but cannot answer “who trained these weights?” on its own. ONNX supports an optional checksum for external tensor data, and TensorFlow SavedModel’s fingerprint.pb provides hash-like fingerprints identifying SavedModel contents. 

Filesystem timestamps are especially weak evidence because ordinary copying, extraction, synchronization, touch, archive creation and conversion can alter them.

Things passive inspection cannot reliably infer
A single weight artifact generally cannot establish, absent explicit authenticated records:

Question	Why it is not reliably recoverable
Exact training dataset	Many corpora can produce statistically similar parameter tensors
Exact examples memorized during training	Weight inspection alone is not a reliable membership oracle
Exact number of training tokens	Usually not encoded in inference weights
Exact optimizer/training schedule	Optimizer state may have been stripped
Whether data was licensed	A weight file does not encode legal rights to its source corpus
Whether a model is genuinely “from” the claimed organization	Metadata can be edited
Whether it was distilled	Distillation can leave no unique serialization signature
Whether it was RLHF/DPO/SFT trained	Weight statistics alone do not uniquely identify the method
Model quality/safety	Requires behavioral evaluation, not just binary parsing
Original full-precision weights from a lossy quantization	Information has generally been discarded
Intent behind removed/zeroed layers	Anomaly detection can find the change, not the author’s motive
Semantic equivalence of two checkpoints	Permutations, folding/fusion, quantization and graph rewrites can change bytes while preserving behavior

The right evidentiary language is therefore “consistent with,” “directly encoded,” “strongly suggests,” or “cannot distinguish”, rather than treating statistical fingerprints as proof.

Detecting quantization, pruning, weight sharing, and alterations
Build a tensor census before drawing conclusions
For every tensor, collect at least:

text
Copy
name
logical shape
element count
stored dtype / quantization type
byte offset / stored byte count
min / max, after valid decoding
mean / standard deviation
L1 norm
L2 or Frobenius norm
exact zero count
unique-value estimate
NaN / Inf count
per-tensor cryptographic hash
For block quantizers, also collect:

text
Copy
group or block size
scale distribution
zero-point/minimum distribution
packed-code histogram
effective bits per weight
fraction of tensors left in FP16/BF16/FP32
Then compare layers belonging to the same architectural role. Cross-layer comparisons are generally more diagnostic than looking at one global histogram.

Detecting pruning
Unstructured magnitude pruning frequently produces exact zeros. Apple describes conventional pruning precisely as setting selected weight values to zero; LiteRT likewise treats pruning as creating sparse weights that are more compressible. 

A simple sparsity statistic is:

[ s = \frac{#{w_i=0}}{N}. ]

Do not stop there. Compute sparsity along multiple axes:

[ s_r(i)=\frac{#\text{zeros in row } i}{\text{row width}} ]

and analogously for columns and fixed-size blocks.

Patterns can distinguish likely representations:

text
Copy
Scattered individual zero values   → unstructured sparsity candidate
Whole zero rows/columns            → structured neuron/channel pruning candidate
Repeated zero blocks               → block pruning candidate
Entire tensor almost/all zero      → disabled/ablated component candidate
No exact zeros but tiny weights    → pruning mask may not have been materialized,
                                     or model may merely be highly regularized
An important quantized-model caveat is that the integer code 0 does not necessarily represent real value zero. For affine quantization,

[ x \approx s(q-z), ]

so tests should run on dequantized logical values where practical. Google’s LiteRT documentation explicitly exposes scale and zero-point parameters for this mapping. 

Detecting quantization
Start with metadata. A GGUF tensor has an explicit GGML type, an ONNX tensor has a standardized dtype and may carry quantization nodes/parameters, and TFLite exposes integer tensor types plus quantization scale/zero-point. 

If metadata is missing or suspicious, inspect numerical structure.

For a hypothetical affine uniform quantizer, test how closely values conform to a candidate lattice:

[ r_i = \left| \frac{w_i}{s}
\operatorname{round}\left(\frac{w_i}{s}\right) \right|. ]

A very small residual distribution for some (s) is evidence of a quantization grid. For affine zero-point quantization, adjust by (z).

Useful visual diagnostics include:

text
Copy
Histogram of dequantized values     → spikes at reconstruction levels
Unique-value count                  → unexpectedly small alphabet
Successive-value spacing            → regular quantization grid
Packed nibble histogram             → 4-bit code utilization
Per-block scale histogram           → groupwise quantization footprint
Value vs. quantization residual     → rounding structure
For codebook/palettized models, look for a small set of centroids plus a large array of indices. Apple describes palettization exactly this way: similar weights are grouped around centroids, with a lookup table and index representation; (n)-bit indices address up to (2^n) centroids. 

Entropy tests
For a byte sequence, Shannon entropy is:

[ H=-\sum_{i=0}^{255}p_i\log_2p_i, ]

bounded by eight bits per byte.

Interpretation needs restraint:

low entropy may identify large zero runs, repeated quantized codes, padding or sparse structure;
codebook indices may have substantially lower symbol entropy when only part of the code space is used;
compressed or encrypted data can approach eight bits per byte;
ordinary floating-point tensor bytes can also exhibit high byte entropy, so high entropy is not proof of encryption or obfuscation.
Calculate entropy per tensor or block, not just per whole file. Headers, JSON, token strings, padding and weights have radically different expected statistics.

A useful comparison metric between a candidate and known baseline is:

[ \Delta H_\ell=H(\text{candidate layer }\ell)-H(\text{baseline layer }\ell). ]

Large localized differences deserve examination but are not self-explanatory.

Detecting weight sharing
There are several meanings of “weight sharing.”

Serialization-level aliasing: two tensor objects reference the same underlying storage. PyTorch serialization can preserve storage/view relationships, so storage IDs and offsets are directly useful here. 

Exact duplicated tensors: hash each tensor’s canonical raw or logical representation. Equal SHA-256 hashes plus equal shape/dtype strongly indicate exact duplicates.

Tied embeddings: compare the input embedding and language-model output projection. Some model families genuinely tie these weights. Depending on serialization, they may be one shared storage or two identical copies.

Codebook sharing: quantizers or palettization can represent many scalar positions using a common set of centroids. That is a different form of sharing from tying two network layers.

A hash duplicate detector conceptually looks like:

python
Copy
digest = SHA256(dtype || shape || canonical_tensor_bytes)
Including shape and dtype avoids treating equal byte strings with different interpretations as equivalent.

Detecting ablation or suspicious structural alteration
Ablation is much easier to establish relative to a baseline.

Compare:

[ R_\ell = \frac{|W^{\text{candidate}}_\ell|F} {|W^{\text{baseline}}\ell|_F}. ]

Potential flags include:

text
Copy
expected tensor absent
extra tensor unexpectedly introduced
tensor replaced by zeros
one attention/MLP projection has anomalously tiny norm
graph edge bypasses an expected submodule
one layer has dramatically changed singular spectrum
model config says N layers but graph/weights contain fewer
specific head/channel groups are exactly zeroed
Across same-role layers, robustly score norms:

[ z_\ell^{MAD}= \frac{x_\ell-\mathrm{median}(x)} {1.4826,\mathrm{MAD}(x)}. ]

MAD-based scores tolerate occasional legitimate outliers better than ordinary mean/stddev z-scores.

For matrices, singular-value profiles can detect near-rank collapse:

[ W=U\Sigma V^\top. ]

An ablated or low-rank-modified layer may have a sharp drop in effective rank, but low-rank structure itself is not proof of tampering: LoRA merges, compression, model design and training can all cause it.

Detecting obfuscation or packing
Signals worth investigating include:

no recognizable format magic despite an expected standard format;
stripped or randomized tensor names;
an almost entirely high-entropy opaque blob with a small loader stub;
encrypted/compressed sections instead of normal tensor structures;
unexpected executable code/native libraries adjacent to weights;
tensor shapes reconstructed only at runtime;
intentionally reordered/permuted dimensions;
weights XORed or transformed and reconstructed by custom code.
But these observations have benign explanations. Platform deployment toolchains routinely compile and pack models. MLC deliberately separates platform-specific compiled code from parameter shards; Core ML packages separate model structure and weights; optimization pipelines can fuse operators and transform weights. 

Thus “not human-readable” is not equivalent to “maliciously obfuscated.”

Layer-wise plots that provide real diagnostic value
For a transformer with (L) blocks, plot independently:

text
Copy
layer index → ||attention Q||F
layer index → ||attention K||F
layer index → ||attention V||F
layer index → ||attention O||F
layer index → ||MLP gate/up/down||F
layer index → fraction exactly zero
layer index → unique values / number of values
layer index → byte entropy
layer index → effective rank
layer index → delta-norm against trusted baseline
A local discontinuity—say, block 31 with 95% zero weights while neighboring blocks are around 0%—is much more interesting than a single global sparsity number.

For two compatible models, normalized difference is useful:

[ d_\ell = \frac{|W_{\ell,A}-W_{\ell,B}|F} {|W{\ell,A}|_F+\epsilon}. ]

Use this only after accounting for tensor transposition, quantization/dequantization, possible permutations, tied tensors and architecture conversion.

Practical inspection workflow and tools
A defensible workflow separates identification, non-executing structural parsing, numerical examination, and behavioral testing.

Safetensors

PyTorch

ONNX

GGUF

SavedModel

TFLite

Core ML

HF/MLC directory

Yes

No

Acquire model read-only

Record file list, sizes, timestamps

Compute cryptographic hashes

file / xxd / hexdump / strings

Identify format

Read JSON header

List ZIP members first

Parse ModelProto without external data

Dump KV + tensor descriptors

saved_model_cli / protobuf + checkpoint reader

FlatBuffer analyzer

Read package/protobuf spec

Parse JSON/index sidecars

Create tensor census

Per-tensor hashes + numerical stats

Histograms / sparsity / entropy / norms

Trusted baseline exists?

Layer-by-layer structural and numeric diff

Internal anomaly analysis

State conclusions with confidence levels



Show code
Shell-level triage
Portable command names vary somewhat by OS, but common Unix-like tools illustrate the workflow:

bash
Copy
# Identity and basic filesystem evidence
file model.*
ls -lh
du -sh .
stat model.safetensors

# Cryptographic content identity
sha256sum model.safetensors
# macOS commonly:
shasum -a 256 model.safetensors

# First 256 bytes
hexdump -C -n 256 model.gguf
xxd -l 256 model.onnx

# Printable strings are hints, not a parser
strings -n 8 model.gguf | head -200

# Is this a ZIP-family object?
unzip -l checkpoint.pth
zipinfo checkpoint.pth
For a directory artifact, inventory before loading:

bash
Copy
find model-dir -type f -print
find model-dir -type f -exec ls -l {} \;
strings is particularly useful for locating obvious architecture names, URLs, tokenizer fragments, licenses or compiler identifiers, but a string hit can be accidental and should be validated with the actual format parser.

Passive Safetensors header extraction
No ML runtime is required:

python
Copy
from __future__ import annotations

import json
import struct
from pathlib import Path


def inspect_safetensors_header(path: str | Path) -> dict:
    path = Path(path)

    with path.open("rb") as f:
        raw_n = f.read(8)
        if len(raw_n) != 8:
            raise ValueError("File too short for a Safetensors header")

        (header_len,) = struct.unpack("<Q", raw_n)

        # Defensive limit for corrupt/adversarial files.
        if header_len > 100_000_000:
            raise ValueError(f"Implausibly large header: {header_len}")

        raw_header = f.read(header_len)
        if len(raw_header) != header_len:
            raise ValueError("Truncated Safetensors header")

    header = json.loads(raw_header)

    for name, info in header.items():
        if name == "__metadata__":
            print("metadata:", info)
            continue

        print(
            name,
            "dtype=", info.get("dtype"),
            "shape=", info.get("shape"),
            "offsets=", info.get("data_offsets"),
        )

    return header


inspect_safetensors_header("model.safetensors")
This directly follows the official format: eight-byte little-endian header length, UTF-8 JSON, then tensor payload. 

With the official Python package:

python
Copy
from safetensors import safe_open

with safe_open("model.safetensors", framework="pt", device="cpu") as f:
    print("metadata:", f.metadata())
    for key in f.keys():
        tensor = f.get_tensor(key)
        print(key, tuple(tensor.shape), tensor.dtype)
Hugging Face configuration inspection without loading weights
python
Copy
from transformers import AutoConfig, AutoTokenizer

path = "./model"

config = AutoConfig.from_pretrained(
    path,
    local_files_only=True,
    trust_remote_code=False,
)

print(config.to_dict())

tokenizer = AutoTokenizer.from_pretrained(
    path,
    local_files_only=True,
    trust_remote_code=False,
)

print("vocab size:", len(tokenizer))
print("special tokens:", tokenizer.special_tokens_map)
trust_remote_code=False is an important passive-inspection default. Transformers can support repository-supplied custom modeling/tokenizer code, but running it turns static inspection into code execution. Hugging Face specifically documents trust_remote_code for custom models and recommends care around remote custom code. 

For the shard mapping itself, no Transformers import is necessary:

bash
Copy
python -m json.tool model.safetensors.index.json
or:

bash
Copy
jq '.metadata, .weight_map' model.safetensors.index.json
Hugging Face documents the index’s metadata plus parameter-to-shard weight_map. 

accelerate is useful when you need to instantiate a very large architecture skeleton on a meta device or subsequently dispatch a checkpoint without first constructing a full duplicate in RAM, but for pure passive inspection, direct config and tensor-header parsing is preferable because it does less work and has a smaller execution surface.

Safer PyTorch checkpoint inspection
Start with the ZIP structure:

python
Copy
import zipfile

with zipfile.ZipFile("checkpoint.pth") as zf:
    for info in zf.infolist():
        print(info.filename, info.file_size)
This lets you establish that the file resembles modern torch.save without unpickling it.

For a trusted or appropriately sandboxed checkpoint compatible with modern restricted loading:

python
Copy
import torch

obj = torch.load(
    "checkpoint.pth",
    map_location="cpu",
    weights_only=True,
    mmap=True,
)

if isinstance(obj, dict):
    for name, value in obj.items():
        if torch.is_tensor(value):
            print(name, tuple(value.shape), value.dtype)
        else:
            print(name, type(value).__name__)
The reason for the caution is structural: ordinary PyTorch serialization uses pickle for the object graph, while tensor storages are handled separately. .pt and .pth do not imply “safe data-only file.” 

Avoid reflexively doing:

python
Copy
torch.load("random-file-from-the-internet.pth")
with permissive pickle semantics on your ordinary workstation.

ONNX metadata extraction
python
Copy
from pathlib import Path
import onnx

path = Path("model.onnx")

model = onnx.load(
    path,
    load_external_data=False,  # inspect references before following them
)

print("IR version:", model.ir_version)
print("producer:", model.producer_name, model.producer_version)
print("model version:", model.model_version)
print("domain:", model.domain)

print("opsets:")
for opset in model.opset_import:
    print(" ", opset.domain or "ai.onnx", opset.version)

print("metadata:")
for item in model.metadata_props:
    print(" ", item.key, "=", item.value)

print("initializers:")
for t in model.graph.initializer:
    print(
        t.name,
        "dims=", list(t.dims),
        "dtype=", t.data_type,
        "raw_bytes=", len(t.raw_data),
        "location=", t.data_location,
    )

print("nodes:", len(model.graph.node))
for node in model.graph.node[:25]:
    print(node.op_type, list(node.input), "->", list(node.output))
The official ONNX IR specification also notes that a protobuf-level dump can be produced using protoc --decode=onnx.ModelProto ..., and specifically mentions Netron as a visualization route. 

Run structural validation separately:

python
Copy
import onnx

model = onnx.load("model.onnx", load_external_data=False)
onnx.checker.check_model(model)
TensorFlow SavedModel
The standard TensorFlow CLI can show signatures without writing application code:

bash
Copy
saved_model_cli show \
  --dir ./saved_model \
  --all
It exposes signature names, input/output tensor specifications and other SavedModel information. 

For variables:

python
Copy
import tensorflow as tf

reader = tf.train.load_checkpoint("./saved_model/variables/variables")

shape_map = reader.get_variable_to_shape_map()
dtype_map = reader.get_variable_to_dtype_map()

for name in sorted(shape_map):
    print(name, shape_map[name], dtype_map[name])
The checkpoint-reader API is preferable to attempting to reverse-engineer the variables.* binary storage yourself. 

For a truly passive protobuf-only pass, parse saved_model.pb through TensorFlow’s generated protobuf classes rather than invoking exported functions.

TFLite / LiteRT
A convenient structural analyzer is:

python
Copy
import tensorflow as tf

tf.lite.experimental.Analyzer.analyze(
    model_path="model.tflite",
)
For tensor-level runtime metadata:

python
Copy
import tensorflow as tf

interpreter = tf.lite.Interpreter(model_path="model.tflite")
interpreter.allocate_tensors()

for d in interpreter.get_tensor_details():
    print(
        d["name"],
        d["shape"],
        d["dtype"],
        d.get("quantization_parameters"),
    )
For hostile/untrusted inputs where the goal is strictly static parsing, prefer a FlatBuffer schema reader or a sandboxed analyzer rather than constructing a runtime interpreter.

Metadata can include semantic information such as model description/license, input/output descriptions and associated label/vocabulary files. Google’s tools can also render that metadata as JSON. 

GGUF and llama.cpp
The official llama.cpp repository’s gguf-py package provides exactly the tools useful here. It documents a reader, gguf_dump.py, endian conversion, metadata editing and a GUI editor. 

After installing/building the package, an installation-independent invocation is:

bash
Copy
python /path/to/llama.cpp/gguf-py/gguf/scripts/gguf_dump.py model.gguf
For custom Python work:

python
Copy
from gguf import GGUFReader

reader = GGUFReader("model.gguf")

for field_name, field in reader.fields.items():
    print(field_name, field)

for tensor in reader.tensors:
    print(
        tensor.name,
        tensor.shape,
        tensor.tensor_type,
    )
Because GGUF metadata explicitly includes the architecture and, for quantized models, a quantization version, this should generally be inspected before attempting heuristics on raw tensor bytes. 

MLC
MLC’s bundle is unusually friendly to first-pass textual inspection:

bash
Copy
find ./model -maxdepth 2 -type f -print

jq . ./model/mlc-chat-config.json
jq . ./model/tensor-cache.json

ls -lh ./model/params_shard_*.bin
Do not assign tensor dimensions to offsets in params_shard_*.bin merely from byte patterns. tensor-cache.json is the weight index, while MLC’s compiled library/config defines the corresponding runtime interpretation. 

Core ML
First inspect the package itself:

bash
Copy
find Model.mlpackage -type f -print
For a standalone protobuf .mlmodel, coremltools exposes the specification:

python
Copy
import coremltools as ct

model = ct.models.MLModel(
    "Model.mlmodel",
    skip_model_load=True,
)

spec = model.get_spec()

print(spec.description)
print(spec.specificationVersion)
skip_model_load=True is useful when the goal is specification inspection rather than compiling/loading it into the Core ML runtime. Package internals and compression operators should be interpreted with the relevant Core ML specification/coremltools version because newer weight compression techniques include sparse, quantized and palettized encodings. Apple explicitly documents those compression families. 

Netron
Netron is extremely effective for graph-first reconnaissance:

bash
Copy
pip install netron
netron model.onnx
or:

bash
Copy
netron model.tflite
Its official project lists full support for ONNX, TensorFlow Lite, PyTorch/TorchScript, TensorFlow, Core ML, Keras and Safetensors, with experimental GGUF support as of the current project documentation. 

Use Netron as a visualization and navigation tool, not as the sole evidentiary source for byte-level conclusions.

Generic numerical-statistics script
For formats you can safely convert into NumPy arrays one tensor at a time:

python
Copy
from __future__ import annotations

import hashlib
import math
from collections import Counter

import numpy as np


def byte_entropy(data: bytes) -> float:
    """Shannon entropy in bits per byte."""
    if not data:
        return 0.0

    counts = Counter(data)
    n = len(data)

    return -sum(
        (c / n) * math.log2(c / n)
        for c in counts.values()
    )


def tensor_stats(name: str, array: np.ndarray) -> dict:
    a = np.asarray(array)

    # Work in float64 for summary statistics, but retain raw bytes for hashing.
    flat = a.reshape(-1)
    finite = np.isfinite(flat) if np.issubdtype(a.dtype, np.number) else None

    raw = a.tobytes(order="C")
    result = {
        "name": name,
        "shape": tuple(a.shape),
        "dtype": str(a.dtype),
        "elements": int(a.size),
        "bytes": int(a.nbytes),
        "sha256": hashlib.sha256(raw).hexdigest(),
        "byte_entropy": byte_entropy(raw),
    }

    if a.size and np.issubdtype(a.dtype, np.number):
        x = flat.astype(np.float64, copy=False)
        result.update(
            zero_fraction=float(np.count_nonzero(x == 0) / x.size),
            finite_fraction=float(np.count_nonzero(np.isfinite(x)) / x.size),
            l1=float(np.nansum(np.abs(x))),
            l2=float(np.sqrt(np.nansum(x * x))),
            mean=float(np.nanmean(x)),
            std=float(np.nanstd(x)),
            min=float(np.nanmin(x)),
            max=float(np.nanmax(x)),
        )

        # Exact unique() can consume large amounts of memory.
        if x.size <= 5_000_000:
            result["unique_values"] = int(np.unique(x).size)

    return result
For multi-hundred-billion-parameter models, sample or stream. Do not call np.unique() across a 10-GB tensor unless you have deliberately budgeted the RAM.

A histogram script should also sample:

python
Copy
import numpy as np
import matplotlib.pyplot as plt

x = tensor.reshape(-1)
if x.size > 1_000_000:
    rng = np.random.default_rng(0)
    x = rng.choice(x, size=1_000_000, replace=False)

plt.hist(x, bins=200)
plt.xlabel("Weight value")
plt.ylabel("Count")
plt.title("Tensor weight distribution")
plt.show()
For quantized packed data, histogram the integer codes and dequantized values separately. They answer different questions.

Legal, ethical, and evidentiary limits
Permission and model licenses
Possessing a model file does not imply unrestricted rights to inspect, copy, redistribute, convert or deploy it. A model may be distributed under an open-source license, a source-available community license, an academic/non-commercial license, a commercial agreement, or ordinary proprietary terms. Metadata such as a GGUF general.license, Core ML description, TFLite metadata or repository LICENSE file can help identify the stated license, but the controlling legal terms may exist elsewhere and internal metadata itself can be incorrect. GGUF explicitly accommodates license, author and repository metadata; LiteRT metadata likewise supports author/license descriptions. 

A prudent inspection process records the license and acquisition source alongside the artifact hash before conversion or redistribution.

Passive parsing versus circumvention
Simply parsing an unencrypted format you are authorized to possess is materially different from defeating encryption, license checks, access controls or other technological protection measures.

In the United States, 17 U.S.C. §1201 regulates circumvention of technological measures. It contains a reverse-engineering provision addressing interoperability of independently created computer programs, but that provision has conditions and is not a blanket authorization for arbitrary access-control circumvention. The U.S. Copyright Office publishes the statutory language, including §1201(f)’s interoperability provision. 

The European Union’s Software Directive likewise contains a conditioned decompilation/interoperability provision in Article 6. The applicability of software-specific rules to particular combinations of model weights, runtime code and protected packages can be jurisdiction- and fact-dependent. 

Accordingly, the practical dividing line for this report is: ordinary parsing, hashing and statistical analysis of files you are entitled to access, not instructions for bypassing DRM, credentials or cryptographic access controls.

Trade secrets and confidential artifacts
An accidentally exposed internal checkpoint is not transformed into unrestricted public information merely because someone can download it. In the United States, federal law criminalizes specified unauthorized conduct involving trade secrets under 18 U.S.C. §1832; the statute expressly addresses stealing or unauthorized appropriation/acquisition of trade-secret information under its stated conditions. 

For a corporate or research environment, investigate authorization before examining:

text
Copy
employee-only checkpoints
vendor models supplied under NDA
leaked model artifacts
customer-specific fine-tunes
models extracted from proprietary applications
artifacts containing regulated or confidential data
Privacy and incidental sensitive information
Static metadata can contain more than model weights: absolute training paths, usernames, experiment names, internal repository references, hostnames, dataset identifiers, comments, author emails, tokenizer additions or configuration strings may survive packaging. Treat extracted strings and metadata as potentially sensitive; do not publish them simply because strings found them.

Likewise, finding a suspicious token or string in a tokenizer does not prove that sensitive text was used as a training example. A vocabulary is a tokenization resource, not a training-corpus ledger.

Safety of the inspection environment
Passive inspection is also a cybersecurity problem. The lowest-risk progression is:

text
Copy
bytes
  ↓
hash/container listing
  ↓
format header
  ↓
declarative metadata
  ↓
tensor parsing
  ↓
model runtime loading
  ↓
custom model code
Each step downward executes or trusts more software.

PyTorch’s pickle-based serialization makes this particularly important. Safetensors was deliberately designed as a non-pickle tensor representation, and Hugging Face Transformers preferentially loads Safetensors where available. 

For unknown artifacts, use a disposable environment with no credentials, no writable sensitive mounts and no unnecessary network access before invoking custom loaders. A format parser can itself have bugs, so “data-only format” means lower risk, not zero risk.

Limits of forensic inference
The most important ethical and scientific limitation is not to turn correlations into accusations.

A tensor with 90% zeros is evidence of 90% numerical sparsity. It is not, by itself, proof that someone maliciously crippled a model.

A layer whose hash differs from an official checkpoint is evidence that its serialized content differs. It is not proof of a backdoor.

A file with high entropy is evidence of a broad byte-value distribution. It is not proof of encryption.

A metadata field naming an organization is evidence that the string is present. It is not proof that the organization created the file.

A four-bit model whose parameter histogram has exactly 16 reconstruction values is strong evidence of a sixteen-level quantized or palettized representation for that tensor. It does not tell you which training data or model-conversion intent produced it.

The best analysis therefore separates conclusions into:

Confidence category	Appropriate statement
Directly observed	“The GGUF header declares architecture X and tensor Y has quantization type Q.”
Derived deterministically	“The tensor dimensions contain 16,777,216 logical elements.”
Strong statistical evidence	“The dequantized values occupy sixteen dominant levels, consistent with a four-bit codebook.”
Comparative evidence	“Layer 18 differs from trusted checkpoint A while all other tensors hash identically.”
Hypothesis only	“This may represent an intentional ablation.”
Not inferable	“The binary alone does not establish the training corpus or author’s intent.”

That distinction is particularly important when inspection results are used for security reports, licensing disputes, model provenance claims or allegations of tampering.

The legal discussion here is a technical risk overview, not jurisdiction-specific legal advice. The safest assumptions are that license terms still matter, authorization matters, technological access controls materially change the legal analysis, and statistical inspection does not magically reveal facts that were never encoded in the artifact. 


