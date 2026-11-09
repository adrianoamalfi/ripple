import { cn } from "@/lib/utils"

/** Four ascending bars; `bars` of them are lit. */
export function SignalBars({
  bars,
  className,
}: {
  bars: 0 | 1 | 2 | 3 | 4
  className?: string
}) {
  return (
    <span
      className={cn("inline-flex h-4 items-end gap-0.5", className)}
      role="img"
      aria-label={`${bars} of 4 signal bars`}
    >
      {[1, 2, 3, 4].map((n) => (
        <span
          key={n}
          className={cn(
            "w-1 rounded-[1px]",
            n <= bars ? "bg-primary" : "bg-muted-foreground/25"
          )}
          style={{ height: `${n * 25}%` }}
        />
      ))}
    </span>
  )
}
