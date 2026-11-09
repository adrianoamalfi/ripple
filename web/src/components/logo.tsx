export function Logo({ className }: { className?: string }) {
  return (
    <svg
      viewBox="0 0 32 32"
      fill="none"
      className={className}
      aria-hidden="true"
    >
      <circle cx="16" cy="16" r="3.2" fill="currentColor" />
      <circle
        cx="16"
        cy="16"
        r="7.6"
        stroke="currentColor"
        strokeWidth="1.8"
        opacity="0.8"
      />
      <circle
        cx="16"
        cy="16"
        r="12.6"
        stroke="currentColor"
        strokeWidth="1.8"
        opacity="0.42"
      />
    </svg>
  )
}
