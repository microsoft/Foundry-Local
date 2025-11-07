// Quick test to verify the validation logic works correctly

const testModels = [
  { name: 'gpt-oss-20b-cuda-gpu:1', tags: [] },
  { name: 'openai-whisper-base-cuda-gpu:1', tags: [] },
  { name: 'Phi-4-cuda-gpu:1', tags: ['promptTemplate:something'] },
  { name: 'Phi-3-mini-4k-instruct-cuda-gpu:1', tags: [] },
];

testModels.forEach(model => {
  const baseModelName = model.name.split(':')[0];
  
  if (baseModelName.toLowerCase().startsWith('gpt-oss-')) {
    console.log(`✓ ${model.name} - VALID (gpt-oss exempt)`);
  } else if (baseModelName.toLowerCase().includes('whisper')) {
    console.log(`✓ ${model.name} - VALID (whisper model)`);
  } else {
    const hasPromptTemplate = model.tags.some(tag => tag.startsWith('promptTemplate:'));
    if (hasPromptTemplate) {
      console.log(`✓ ${model.name} - VALID (has promptTemplate)`);
    } else {
      console.log(`✗ ${model.name} - FILTERED OUT (no promptTemplate)`);
    }
  }
});
